//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2023 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include <vast/concept/parseable/to.hpp>
#include <vast/concept/parseable/vast/data.hpp>
#include <vast/detail/fdinbuf.hpp>
#include <vast/detail/file_path_to_parser.hpp>
#include <vast/detail/posix.hpp>
#include <vast/detail/string.hpp>
#include <vast/logger.hpp>
#include <vast/plugin.hpp>

#include <caf/error.hpp>

#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unistd.h>

namespace vast::plugins::file {

class plugin : public virtual loader_plugin {
public:
  static constexpr auto max_chunk_size = size_t{16384};
  static constexpr auto stdin_path = "-";

  auto
  make_loader(std::span<std::string const> args, operator_control_plane&) const
    -> caf::expected<generator<chunk_ptr>> override {
    auto read_timeout = read_timeout_;
    auto path = std::string{};
    auto following = false;
    auto is_socket = false;
    for (auto i = size_t{0}; i < args.size(); ++i) {
      const auto& arg = args[i];
      VAST_TRACE("processing loader argument {}", arg);
      if (arg == "--timeout") {
        if (i + 1 == args.size()) {
          return caf::make_error(ec::syntax_error,
                                 fmt::format("missing duration value"));
        }
        if (auto parsed_duration = to<vast::duration>(args[i + 1])) {
          read_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
            *parsed_duration);
          ++i;
        } else {
          return caf::make_error(ec::syntax_error,
                                 fmt::format("could not parse duration: {}",
                                             args[i + 1]));
        }
      } else if (arg == "-" || arg == "stdin") {
        path = stdin_path;
      } else if (arg == "-f" || arg == "--follow") {
        following = true;
      } else if (not arg.starts_with("-")) {
        std::error_code err{};
        auto status = std::filesystem::status(arg, err);
        if (err) {
          return caf::make_error(ec::parse_error,
                                 fmt::format("could not access file {}: {}",
                                             arg, err));
        }
        is_socket = (status.type() == std::filesystem::file_type::socket);
        path = arg;
      } else {
        return caf::make_error(
          ec::invalid_argument,
          fmt::format("unexpected argument for 'file' connector: {}", arg));
      }
    }
    if (path.empty()) {
      return caf::make_error(ec::syntax_error,
                             fmt::format("no file specified"));
    }
    auto fd = STDIN_FILENO;
    if (is_socket) {
      if (path == stdin_path) {
        return caf::make_error(ec::filesystem_error, "cannot use STDIN as UNIX "
                                                     "domain socket");
      }
      auto uds = detail::unix_domain_socket::connect(path);
      if (!uds) {
        return caf::make_error(ec::filesystem_error,
                               "failed to connect to UNIX domain socket at",
                               path);
      }
      fd = uds.recv_fd();
      if (fd == -1) {
        return caf::make_error(
          ec::filesystem_error,
          "Unable to connect to UNIX domain socket at {}:", path);
      }
    } else {
      if (path != stdin_path) {
        fd = ::open(path.c_str(), std::ios_base::binary | std::ios_base::in);
        if (fd == -1) {
          return caf::make_error(ec::filesystem_error,
                                 "open(2) for file {} failed {}:", path,
                                 std::strerror(errno));
        }
      }
    }
    return std::invoke(
      [](auto timeout, auto fd, auto following) -> generator<chunk_ptr> {
        auto in_buf = detail::fdinbuf(fd, max_chunk_size);
        in_buf.read_timeout() = timeout;
        auto current_data = std::vector<std::byte>{};
        current_data.reserve(max_chunk_size);
        auto eof_reached = false;
        while (following or not eof_reached) {
          auto current_char = in_buf.sbumpc();
          if (current_char != detail::fdinbuf::traits_type::eof()) {
            current_data.emplace_back(static_cast<std::byte>(current_char));
          } else {
            eof_reached = (not in_buf.timed_out());
            if (current_data.empty()) {
              if (not eof_reached) {
                co_yield chunk::make_empty();
                continue;
              }
              if (eof_reached and not following) {
                break;
              }
            }
          }
          if (eof_reached or current_data.size() == max_chunk_size) {
            auto chunk = chunk::make(std::exchange(current_data, {}));
            co_yield std::move(chunk);
            if (following or not eof_reached) {
              current_data.reserve(max_chunk_size);
            }
          }
        }
        if (fd != STDIN_FILENO) {
          ::close(fd);
        }
        co_return;
      },
      read_timeout, fd, following);
  }

  auto default_parser(std::span<std::string const> args) const
    -> std::pair<std::string, std::vector<std::string>> override {
    for (std::string_view arg : args) {
      if (arg == "-" || arg == "stdin") {
        break;
      }
      if (!arg.starts_with("-")) {
        return {detail::file_path_to_parser(arg), {}};
      }
    }
    return {"json", {}};
  }

  auto initialize(const record&, const record& global_config)
    -> caf::error override {
    const auto* read_timeout_entry
      = get_if<std::string>(&global_config, "vast.import.read-timeout");
    if (!read_timeout_entry) {
      return caf::none;
    }
    if (auto timeout_duration = to<vast::duration>(*read_timeout_entry)) {
      read_timeout_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        *timeout_duration);
    }
    return caf::none;
  }

  auto name() const -> std::string override {
    return "file";
  }

private:
  std::chrono::milliseconds read_timeout_{vast::defaults::import::read_timeout};
};

} // namespace vast::plugins::file

namespace vast::plugins::stdin_ {

class plugin : public virtual vast::plugins::file::plugin {
public:
  auto make_loader([[maybe_unused]] std::span<std::string const> args,
                   operator_control_plane& ctrl) const
    -> caf::expected<generator<chunk_ptr>> override {
    std::vector<std::string> new_args = {args.begin(), args.end()};
    new_args.emplace_back("-");
    return vast::plugins::file::plugin::make_loader(new_args, ctrl);
  }

  auto default_parser([[maybe_unused]] std::span<std::string const> args) const
    -> std::pair<std::string, std::vector<std::string>> override {
    return {"json", {}};
  }

  auto name() const -> std::string override {
    return "stdin";
  }
};

} // namespace vast::plugins::stdin_

VAST_REGISTER_PLUGIN(vast::plugins::file::plugin)
VAST_REGISTER_PLUGIN(vast::plugins::stdin_::plugin)