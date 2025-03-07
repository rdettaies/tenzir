//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/format/reader_factory.hpp"

#include "tenzir/config.hpp"
#include "tenzir/detail/make_io_stream.hpp"
#include "tenzir/format/arrow.hpp"
#include "tenzir/format/csv.hpp"
#include "tenzir/format/json.hpp"
#include "tenzir/format/json/default_selector.hpp"
#include "tenzir/format/json/suricata_selector.hpp"
#include "tenzir/format/json/zeek_selector.hpp"
#include "tenzir/format/reader.hpp"
#include "tenzir/format/syslog.hpp"
#include "tenzir/format/test.hpp"
#include "tenzir/format/zeek.hpp"
#include "tenzir/logger.hpp"
#include "tenzir/plugin.hpp"

namespace tenzir {

template <class Reader, class Selector = void>
caf::expected<std::unique_ptr<format::reader>>
make_reader(caf::settings options) {
  if constexpr (!std::is_void_v<Selector>) {
    static_assert(std::is_same_v<Reader, format::json::reader>,
                  "selectors are currently only implemented for the JSON "
                  "reader");
    caf::put(options, "tenzir.import.json.selector",
             fmt::format("{}:{}", Selector::field_name, Selector::type_prefix));
    // If the user did not provide a type restriction, we can use the type
    // prefix to restrict the types as a good default.
    if (!caf::holds_alternative<std::string>(options, "tenzir.import.type"))
      caf::put(options, "tenzir.import.type", Selector::type_prefix);
  }
  using istream_ptr = std::unique_ptr<std::istream>;
  if constexpr (std::is_constructible_v<Reader, caf::settings, istream_ptr>) {
    auto in = detail::make_input_stream(options);
    if (!in)
      return in.error();
    return std::make_unique<Reader>(options, std::move(*in));
  } else {
    return std::make_unique<Reader>(options);
  }
}

void factory_traits<format::reader>::initialize() {
  using namespace format;
  using fac = factory<reader>;
  fac::add("arrow", make_reader<arrow::reader>);
  fac::add("csv", make_reader<csv::reader>);
  fac::add("json", make_reader<json::reader>);
  fac::add("suricata",
           make_reader<format::json::reader, json::suricata_selector>);
  fac::add("syslog", make_reader<syslog::reader>);
  fac::add("test", make_reader<test::reader>);
  fac::add("zeek", make_reader<zeek::reader>);
  fac::add("zeek-json", make_reader<json::reader, json::zeek_selector>);
  for (const auto& plugin : plugins::get()) {
    if (const auto* reader = plugin.as<reader_plugin>()) {
      fac::add(
        reader->reader_format(),
        [name = std::string{plugin->name()}](const caf::settings& options)
          -> caf::expected<std::unique_ptr<format::reader>> {
          for (const auto& plugin : plugins::get()) {
            if (plugin->name() != name)
              continue;
            const auto* reader = plugin.as<reader_plugin>();
            TENZIR_ASSERT(reader);
            return reader->make_reader(options);
          }
          return caf::make_error(ec::logic_error,
                                 fmt::format("reader plugin {} was used to "
                                             "initialize factory but unloaded "
                                             "at a later point in time",
                                             name));
        });
    }
  }
}

} // namespace tenzir
