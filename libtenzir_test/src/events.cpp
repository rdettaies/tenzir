//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2018 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "fixtures/events.hpp"

#include "tenzir/concept/parseable/tenzir/schema.hpp"
#include "tenzir/concept/parseable/to.hpp"
#include "tenzir/concept/printable/tenzir/data.hpp"
#include "tenzir/concept/printable/to_string.hpp"
#include "tenzir/defaults.hpp"
#include "tenzir/detail/assert.hpp"
#include "tenzir/format/json.hpp"
#include "tenzir/format/test.hpp"
#include "tenzir/format/zeek.hpp"
#include "tenzir/table_slice_builder.hpp"
#include "tenzir/type.hpp"

#include <caf/settings.hpp>
#include <caf/test/dsl.hpp>

// Pull in the auto-generated serialized table slices.

namespace artifacts::logs::zeek {

extern char conn_buf[];

extern size_t conn_buf_size;

} // namespace artifacts::logs::zeek

namespace fixtures {

namespace {

struct ascending {};
struct alternating {};

template <class Policy>
std::vector<table_slice> make_integers(size_t count) {
  auto schema = type{"test.int", record_type{{"value", int64_type{}}}};
  auto builder = std::make_shared<table_slice_builder>(schema);
  TENZIR_ASSERT(builder != nullptr);
  std::vector<table_slice> result;
  result.reserve(count);
  auto i = size_t{0};
  while (i < count) {
    int64_t x;
    if constexpr (std::is_same_v<Policy, ascending>)
      x = i;
    else if constexpr (std::is_same_v<Policy, alternating>)
      x = i % 2;
    else
      static_assert(detail::always_false_v<Policy>, "invalid policy");
    if (!builder->add(make_view(x)))
      FAIL("could not add data to builder at row" << i);
    if (++i % events::slice_size == 0)
      result.push_back(builder->finish());
  }
  // Add last slice.
  if (i % events::slice_size != 0)
    result.push_back(builder->finish());
  TENZIR_ASSERT(!result.empty());
  return result;
}

template <class Reader>
std::vector<table_slice>
extract(Reader&& reader, table_slice::size_type slice_size) {
  std::vector<table_slice> result;
  auto add_slice = [&](table_slice slice) {
    result.emplace_back(std::move(slice));
  };
  auto [err, produced]
    = reader.read(std::numeric_limits<size_t>::max(), slice_size, add_slice);
  if (err && err != ec::end_of_input)
    FAIL("reader returned an error: " << to_string(err));
  return result;
}

template <class Reader>
std::vector<table_slice>
inhale(const char* filename, table_slice::size_type slice_size) {
  caf::settings settings;
  // A non-positive value disables the timeout. We need to do this because the
  // deterministic actor system is messing with the clocks.
  caf::put(settings, "tenzir.import.batch-timeout", "0s");
  auto input = std::make_unique<std::ifstream>(filename);
  Reader reader{settings, std::move(input)};
  return extract(reader, slice_size);
}

template <>
std::vector<table_slice>
inhale<format::json::reader>(const char* filename,
                             table_slice::size_type slice_size) {
  caf::settings settings;
  // A non-positive value disables the timeout. We need to do this because the
  // deterministic actor system is messing with the clocks.
  caf::put(settings, "tenzir.import.batch-timeout", "0s");
  caf::put(settings, "tenzir.import.json.selector", "event_type:suricata");
  auto input = std::make_unique<std::ifstream>(filename);
  format::json::reader reader{settings, std::move(input)};
  REQUIRE_EQUAL(reader.module(events::suricata_module), caf::error{});
  return extract(reader, slice_size);
}

} // namespace

std::vector<table_slice> events::zeek_conn_log;
std::vector<table_slice> events::zeek_conn_log_full;
std::vector<table_slice> events::zeek_dns_log;
std::vector<table_slice> events::zeek_http_log;
std::vector<table_slice> events::random;
std::vector<table_slice> events::suricata_alert_log;
std::vector<table_slice> events::suricata_dns_log;
std::vector<table_slice> events::suricata_fileinfo_log;
std::vector<table_slice> events::suricata_flow_log;
std::vector<table_slice> events::suricata_http_log;
std::vector<table_slice> events::suricata_netflow_log;
std::vector<table_slice> events::suricata_stats_log;
std::vector<table_slice> events::ascending_integers;
std::vector<table_slice> events::alternating_integers;
tenzir::module events::suricata_module;

events::events() {
  // Only read the fixture data once per process.
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  // Read schemas
  std::ifstream base(artifacts::schemas::base);
  std::stringstream buffer;
  buffer << base.rdbuf();
  auto base_raw_schema = buffer.str();
  std::ifstream suricata(artifacts::schemas::suricata);
  std::stringstream().swap(buffer);
  buffer << suricata.rdbuf();
  auto suricata_raw_schema = buffer.str();
  suricata_module
    = unbox(to<tenzir::module>(base_raw_schema + suricata_raw_schema));
  // Create Zeek log data.
  MESSAGE("inhaling unit test suite events");
  zeek_conn_log = inhale<format::zeek::reader>(
    artifacts::logs::zeek::small_conn, slice_size);
  REQUIRE_EQUAL(rows(zeek_conn_log), 20u);
  auto&& schema = zeek_conn_log[0].schema();
  CHECK_EQUAL(schema.name(), "zeek.conn");
  zeek_dns_log
    = inhale<format::zeek::reader>(artifacts::logs::zeek::dns, slice_size);
  REQUIRE_EQUAL(rows(zeek_dns_log), 32u);
  zeek_http_log
    = inhale<format::zeek::reader>(artifacts::logs::zeek::http, slice_size);
  REQUIRE_EQUAL(rows(zeek_http_log), 40u);
  // For the full conn.log, we're using a different table slice size for
  // historic reasons: there used to be a utility that generated a binary set
  // of table slices that used a different table slice size than the other
  // table slice collections.
  zeek_conn_log_full
    = inhale<format::zeek::reader>(artifacts::logs::zeek::conn, 100u);
  REQUIRE_EQUAL(rows(zeek_conn_log_full), 8462u);
  // Create random table slices.
  caf::settings opts;
  caf::put(opts, "tenzir.import.test.seed", std::size_t{42});
  caf::put(opts, "tenzir.import.max-events", std::size_t{1000});
  tenzir::format::test::reader rd{std::move(opts), nullptr};
  random = extract(rd, slice_size);
  REQUIRE_EQUAL(rows(random), 1000u);
  // Create integer test data.
  ascending_integers = make_integers<ascending>(250);
  alternating_integers = make_integers<alternating>(250);
  REQUIRE_EQUAL(rows(ascending_integers), 250u);
  REQUIRE_EQUAL(rows(alternating_integers), 250u);
  // Create Suricata log data
  suricata_alert_log = inhale<format::json::reader>(
    artifacts::logs::suricata::alert, slice_size);
  REQUIRE_EQUAL(rows(suricata_alert_log), 1u);
  REQUIRE_EQUAL(suricata_alert_log[0].columns(), 39u);
  suricata_dns_log
    = inhale<format::json::reader>(artifacts::logs::suricata::dns, slice_size);
  REQUIRE_EQUAL(rows(suricata_dns_log), 1u);
  REQUIRE_EQUAL(suricata_dns_log[0].columns(), 37u);
  suricata_fileinfo_log = inhale<format::json::reader>(
    artifacts::logs::suricata::fileinfo, slice_size);
  REQUIRE_EQUAL(rows(suricata_fileinfo_log), 1u);
  REQUIRE_EQUAL(suricata_fileinfo_log[0].columns(), 35u);
  suricata_flow_log
    = inhale<format::json::reader>(artifacts::logs::suricata::flow, slice_size);
  REQUIRE_EQUAL(rows(suricata_flow_log), 1u);
  REQUIRE_EQUAL(suricata_flow_log[0].columns(), 23u);
  suricata_http_log
    = inhale<format::json::reader>(artifacts::logs::suricata::http, slice_size);
  REQUIRE_EQUAL(rows(suricata_http_log), 1u);
  REQUIRE_EQUAL(suricata_http_log[0].columns(), 24u);
  suricata_netflow_log = inhale<format::json::reader>(
    artifacts::logs::suricata::netflow, slice_size);
  REQUIRE_EQUAL(rows(suricata_netflow_log), 1u);
  REQUIRE_EQUAL(suricata_netflow_log[0].columns(), 18u);
  suricata_stats_log = inhale<format::json::reader>(
    artifacts::logs::suricata::stats, slice_size);
  REQUIRE_EQUAL(rows(suricata_stats_log), 1u);
  REQUIRE_EQUAL(suricata_stats_log[0].columns(), 1u);
  // Assign IDs.
  auto assign_ids = [&](auto& slices) {
    auto i = id{0};
    for (auto& slice : slices) {
      slice.offset(i);
      i += slice.rows();
    }
  };
  assign_ids(zeek_conn_log);
  assign_ids(zeek_dns_log);
  assign_ids(zeek_http_log);
  assign_ids(ascending_integers);
  assign_ids(alternating_integers);
  assign_ids(zeek_conn_log_full);
  assign_ids(suricata_alert_log);
  assign_ids(suricata_dns_log);
  assign_ids(suricata_fileinfo_log);
  assign_ids(suricata_flow_log);
  assign_ids(suricata_http_log);
  assign_ids(suricata_netflow_log);
  assign_ids(suricata_stats_log);
}

} // namespace fixtures
