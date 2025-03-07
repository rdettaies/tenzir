//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/partition_synopsis.hpp"

#include "tenzir/bloom_filter_synopsis.hpp"
#include "tenzir/collect.hpp"
#include "tenzir/defaults.hpp"
#include "tenzir/test/fixtures/events.hpp"
#include "tenzir/test/test.hpp"

namespace {

struct fixture : fixtures::events {};

} // namespace

FIXTURE_SCOPE(partition_tests, fixture)

TEST(custom index_config) {
  // Setup.
  using namespace std::string_literals;
  auto ps = tenzir::partition_synopsis{};
  auto capacity = tenzir::defaults::max_partition_size;
  auto synopsis_opts = tenzir::index_config{
    .rules = {
      {
        .targets = {"zeek.http.uri"s},
        .fp_rate = 0.001,
      },
      {
        .targets = {":ip"s},
        .fp_rate = 0.05,
      },
    },
  };
  // Ingest.
  for (const auto& slice : zeek_http_log)
    ps.add(slice, capacity, synopsis_opts);
  ps.shrink();
  // Verify field synopses.
  auto&& schema = zeek_http_log.at(0).schema();
  auto const& schema_rt = caf::get<tenzir::record_type>(schema);
  auto uri_key = schema_rt.resolve_key("uri");
  auto host_key = schema_rt.resolve_key("host");
  REQUIRE(uri_key);
  REQUIRE(host_key);
  auto uri_field = tenzir::qualified_record_field(schema, *uri_key);
  auto host_field = tenzir::qualified_record_field(schema, *host_key);
  auto const& host_synopsis = ps.field_synopses_.at(host_field);
  CHECK_EQUAL(host_synopsis, nullptr);
  auto& url_synopsis = ps.field_synopses_.at(uri_field);
  REQUIRE_NOT_EQUAL(url_synopsis, nullptr);
  auto const& type = url_synopsis->type();
  auto attributes = tenzir::collect(type.attributes());
  auto url_parameters = tenzir::parse_parameters(url_synopsis->type());
  REQUIRE(url_parameters.has_value());
  CHECK_EQUAL(url_parameters->p, 0.001);
  // Verify type synopses.
  auto& string_synopsis
    = ps.type_synopses_.at(tenzir::type{tenzir::string_type{}});
  auto& ip_synopsis = ps.type_synopses_.at(tenzir::type{tenzir::ip_type{}});
  auto string_parameters = tenzir::parse_parameters(string_synopsis->type());
  auto address_parameters = tenzir::parse_parameters(ip_synopsis->type());
  REQUIRE(string_parameters);
  REQUIRE(url_parameters);
  CHECK_EQUAL(string_parameters->p, tenzir::defaults::fp_rate);
  CHECK_EQUAL(address_parameters->p, 0.05);
}

FIXTURE_SCOPE_END()
