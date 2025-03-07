//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/make_source.hpp"

#include "tenzir/command.hpp"
#include "tenzir/component_config.hpp"
#include "tenzir/concept/parseable/tenzir/endpoint.hpp"
#include "tenzir/concept/parseable/tenzir/expression.hpp"
#include "tenzir/concept/parseable/tenzir/schema.hpp"
#include "tenzir/concept/parseable/tenzir/table_slice_encoding.hpp"
#include "tenzir/concept/parseable/to.hpp"
#include "tenzir/concept/printable/tenzir/port.hpp"
#include "tenzir/concept/printable/to_string.hpp"
#include "tenzir/datagram_source.hpp"
#include "tenzir/defaults.hpp"
#include "tenzir/endpoint.hpp"
#include "tenzir/error.hpp"
#include "tenzir/expression.hpp"
#include "tenzir/format/reader.hpp"
#include "tenzir/logger.hpp"
#include "tenzir/module.hpp"
#include "tenzir/optional.hpp"
#include "tenzir/source.hpp"
#include "tenzir/uuid.hpp"

#include <caf/io/middleman.hpp>
#include <caf/settings.hpp>
#include <caf/spawn_options.hpp>

namespace tenzir {

namespace {

template <class... Args>
void send_to_source(caf::actor& source, Args&&... args) {
  static_assert(caf::detail::tl_count_type<
                  source_actor::signatures,
                  auto(std::decay_t<Args>...)->caf::result<void>>::value,
                "Args are incompatible with source actor's API");
  caf::anon_send(source, std::forward<Args>(args)...);
}

} // namespace

caf::expected<caf::actor>
make_source(caf::actor_system& sys, const std::string& format,
            const invocation& inv, accountant_actor accountant,
            catalog_actor catalog,
            stream_sink_actor<table_slice, std::string> importer,
            expression expr, bool detached) {
  if (!importer)
    return caf::make_error(ec::missing_component, "importer");
  // Placeholder thingies.
  auto udp_port = std::optional<uint16_t>{};
  // Parse options.
  const auto& options = inv.options;
  auto max_events = to_optional(
    caf::get_if<caf::config_value::integer>(&options, //
                                            "tenzir.import.max-events"));
  auto uri = caf::get_if<std::string>(&options, "tenzir.import.listen");
  auto file = caf::get_if<std::string>(&options, "tenzir.import.read");
  auto type = caf::get_if<std::string>(&options, "tenzir.import.type");
  auto encoding = defaults::import::table_slice_type;
  if (!extract_settings(encoding, options, "tenzir.import.batch-encoding"))
    return caf::make_error(ec::invalid_configuration, "failed to extract "
                                                      "batch-encoding option");
  TENZIR_ASSERT(encoding != table_slice_encoding::none);
  auto slice_size = caf::get_or(options, "tenzir.import.batch-size",
                                defaults::import::table_slice_size);
  if (slice_size == 0)
    slice_size = std::numeric_limits<decltype(slice_size)>::max();
  // Parse module local to the import command.
  auto module = get_module(options);
  if (!module)
    return module.error();
  // Discern the input source (file, stream, or socket).
  if (uri && file)
    return caf::make_error(ec::invalid_configuration, //
                           "only one source possible (-r or -l)");
  if (uri) {
    endpoint ep;
    if (!tenzir::parsers::endpoint(*uri, ep))
      return caf::make_error(tenzir::ec::parse_error,
                             "unable to parse endpoint", *uri);
    if (!ep.port)
      return caf::make_error(tenzir::ec::invalid_configuration,
                             "endpoint does not "
                             "specify port");
    if (ep.port->type() == port_type::unknown)
      // Fall back to tcp if we don't know anything else.
      ep.port = port{ep.port->number(), port_type::tcp};
    TENZIR_INFO("{}-reader listens for data on {}", format,
                ep.host + ":" + to_string(*ep.port));
    switch (ep.port->type()) {
      default:
        return caf::make_error(tenzir::ec::unimplemented,
                               "port type not supported:", ep.port->type());
      case port_type::udp:
        udp_port = ep.port->number();
        break;
    }
  }
  auto reader = format::reader::make(format, inv.options);
  if (!reader)
    return reader.error();
  if (!*reader)
    return caf::make_error(ec::logic_error, fmt::format("the {} reader is not "
                                                        "registered with the "
                                                        "reader factory",
                                                        format));
  if (slice_size == std::numeric_limits<decltype(slice_size)>::max())
    TENZIR_VERBOSE("{} produces {} table slices", (*reader)->name(), encoding);
  else
    TENZIR_VERBOSE("{} produces {} table slices of at most {} events",
                   (*reader)->name(), encoding, slice_size);
  // Spawn the source, falling back to the default spawn function.
  auto local_module = module ? std::move(*module) : tenzir::module{};
  auto type_filter = type ? std::move(*type) : std::string{};
  auto src =
    [&](auto&&... args) {
      if (udp_port) {
        if (detached)
          return sys.middleman().spawn_broker<caf::spawn_options::detach_flag>(
            datagram_source, *udp_port, std::forward<decltype(args)>(args)...);
        return sys.middleman().spawn_broker(
          datagram_source, *udp_port, std::forward<decltype(args)>(args)...);
      }
      if (detached)
        return sys.spawn<caf::detached>(source,
                                        std::forward<decltype(args)>(args)...);
      return sys.spawn(source, std::forward<decltype(args)>(args)...);
    }(std::move(*reader), slice_size, max_events, std::move(catalog),
      std::move(local_module), std::move(type_filter), std::move(accountant));
  TENZIR_ASSERT(src);
  if (!caf::holds_alternative<caf::none_t>(expr)) {
    send_to_source(src, atom::normalize_v, std::move(expr));
  }
  // Connect source to importer.
  TENZIR_DEBUG("{} connects to {}", inv.full_name, TENZIR_ARG(importer));
  send_to_source(src, importer);
  return src;
}

} // namespace tenzir
