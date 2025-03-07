//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2023 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/adaptive_table_slice_builder.hpp"

#include "tenzir/data.hpp"
#include "tenzir/test/test.hpp"

#include <arrow/record_batch.h>

using namespace tenzir;
using namespace std::chrono_literals;

TEST(add two rows of nested records) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int1").add(int64_t{5}));
    REQUIRE(not row.push_field("str1").add("some_str"));
    auto nested = row.push_field("nested").push_record();
    REQUIRE(not nested.push_field("rec1").add(int64_t{10}));
    REQUIRE(not nested.push_field("rec2").add("rec_str"));
  }
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int1").add(int64_t{5}));
    REQUIRE(not row.push_field("str1").add("some_str"));
    auto nested = row.push_field("nested").push_record();
    REQUIRE(not nested.push_field("rec1").add(int64_t{10}));
    REQUIRE(not nested.push_field("rec2").add("rec_str"));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 4u);
  for (std::size_t i = 0u; i < out.rows(); ++i) {
    CHECK_EQUAL((materialize(out.at(i, 0u))), int64_t{5});
    CHECK_EQUAL((materialize(out.at(i, 1u))), "some_str");
    CHECK_EQUAL((materialize(out.at(i, 2u))), int64_t{10});
    CHECK_EQUAL((materialize(out.at(i, 3u))), "rec_str");
  }
  const auto schema
    = tenzir::type{record_type{{"int1", int64_type{}},
                               {"str1", string_type{}},
                               {"nested", record_type{
                                            {"rec1", int64_type{}},
                                            {"rec2", string_type{}},
                                          }}}};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(add two rows of structs with lists) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto a_f = row.push_field("a");
    auto rec = a_f.push_record();
    auto list_f = rec.push_field("list");
    auto list = list_f.push_list();
    REQUIRE(not list.add(int64_t{1}));
  }
  {
    auto row = sut.push_row();
    auto a_f = row.push_field("a");
    auto rec = a_f.push_record();
    auto list_f = rec.push_field("list");
    auto list = list_f.push_list();
    REQUIRE(not list.add(int64_t{2}));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), list{{int64_t{1}}});
  CHECK_EQUAL((materialize(out.at(1u, 0u))), list{{int64_t{2}}});
  const auto schema = tenzir::type{
    record_type{{"a", record_type{{"list", list_type{int64_type{}}}}}}};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(single row with nested lists) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{5}));
    auto arr_field = row.push_field("arr");
    auto outer_list = arr_field.push_list();
    {
      auto level_1_list = outer_list.push_list();
      {
        auto level_2_list = level_1_list.push_list();
        REQUIRE(not level_2_list.add(int64_t{1}));
        REQUIRE(not level_2_list.add(int64_t{2}));
      }
      {
        auto level_2_list = level_1_list.push_list();
        REQUIRE(not level_2_list.add(int64_t{3}));
        REQUIRE(not level_2_list.add(int64_t{4}));
      }
    }
    {
      auto level_1_list = outer_list.push_list();
      {
        auto level_2_list = level_1_list.push_list();
        REQUIRE(not level_2_list.add(int64_t{5}));
        REQUIRE(not level_2_list.add(int64_t{6}));
      }
      {
        auto level_2_list = level_1_list.push_list();
        REQUIRE(not level_2_list.add(int64_t{7}));
        REQUIRE(not level_2_list.add(int64_t{8}));
      }
    }
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), int64_t{5});
  CHECK_EQUAL(
    (materialize(out.at(0u, 1u))),
    (list{list{list{int64_t{1}, int64_t{2}}, list{int64_t{3}, int64_t{4}}},
          list{list{int64_t{5}, int64_t{6}}, list{int64_t{7}, int64_t{8}}}}));
  const auto schema = tenzir::type{record_type{
    {"int", int64_type{}},
    {"arr", list_type{type{list_type{type{list_type{int64_type{}}}}}}},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(single record with array inside nested record) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("bool").add(true));
    auto nested_rec_field = row.push_field("nested");
    auto nested = nested_rec_field.push_record();
    auto nested_arr_field = nested.push_field("arr");
    auto nested_arr = nested_arr_field.push_list();
    REQUIRE(not nested_arr.add(uint64_t{10}));
    REQUIRE(not nested_arr.add(uint64_t{100}));
    REQUIRE(not nested_arr.add(uint64_t{1000}));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), true);
  CHECK_EQUAL((materialize(out.at(0u, 1u))),
              (list{uint64_t{10}, uint64_t{100}, uint64_t{1000}}));
  const auto schema = tenzir::type{record_type{
    {"bool", bool_type{}},
    {"nested", record_type{{"arr", list_type{uint64_type{}}}}},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(record nested in array of records in two rows) {
  adaptive_table_slice_builder sut;
  const auto row_1_time_point = std::chrono::system_clock::now();
  const auto row_2_time_point = row_1_time_point + std::chrono::seconds{5u};
  {
    auto row = sut.push_row();
    auto arr_field = row.push_field("arr");
    auto arr = arr_field.push_list();
    auto rec = arr.push_record();
    REQUIRE(not rec.push_field("rec double").add(2.0));
    REQUIRE(not rec.push_field("rec time").add(tenzir::time{row_1_time_point}));
    auto nested_rec_fied = rec.push_field("nested rec");
    auto nested_rec = nested_rec_fied.push_record();
    REQUIRE(not nested_rec.push_field("nested duration").add(duration{20us}));
  }
  {
    auto row = sut.push_row();
    auto arr_field = row.push_field("arr");
    auto arr = arr_field.push_list();
    auto rec = arr.push_record();
    REQUIRE(not rec.push_field("rec double").add(4.0));
    REQUIRE(not rec.push_field("rec time").add(tenzir::time{row_2_time_point}));
    auto nested_rec_fied = rec.push_field("nested rec");
    auto nested_rec = nested_rec_fied.push_record();
    REQUIRE(not nested_rec.push_field("nested duration").add(duration{6ms}));
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 2u);
  CHECK_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))),
              (list{record{{"rec double", double{2.0}},
                           {"rec time", tenzir::time{row_1_time_point}},
                           {"nested rec",
                            record{{"nested duration", duration{20us}}}}}}));
  CHECK_EQUAL((materialize(out.at(1u, 0u))),
              (list{record{{{"rec double", double{4.0}},
                            {"rec time", tenzir::time{row_2_time_point}},
                            {"nested rec",
                             record{{{"nested duration", duration{6ms}}}}}}}}));
  const auto schema = tenzir::type{record_type{
    {"arr", list_type{record_type{
              {"rec double", double_type{}},
              {"rec time", time_type{}},
              {"nested rec", record_type{{"nested duration", duration_type{}}}},
            }}}}};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(two rows of array of complex records) {
  const auto row_1_1_subnet = subnet{ip::v4(1u), 1u};
  const auto row_1_2_subnet = subnet{ip::v4(5u), 5u};
  const auto row_2_1_subnet = subnet{ip::v4(0xFF), 10u};
  const auto row_2_2_subnet = subnet{ip::v4(0u), 4u};
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto arr_field = row.push_field("arr");
    auto arr = arr_field.push_list();
    {
      auto rec = arr.push_record();
      REQUIRE(not rec.push_field("subnet").add(row_1_1_subnet));
      auto ip_arr_arr_field = rec.push_field("ip arr");
      auto ip_arr_arr = ip_arr_arr_field.push_list();
      auto ip_arr_1 = ip_arr_arr.push_list();
      REQUIRE(not ip_arr_1.add(ip::v4(2u)));
      REQUIRE(not ip_arr_1.add(ip::v4(3u)));
      REQUIRE(not ip_arr_arr.push_list().add(ip::v4(4u)));
    }
    {
      auto rec = arr.push_record();
      REQUIRE(not rec.push_field("subnet").add(row_1_2_subnet));
      REQUIRE(
        not rec.push_field("ip arr").push_list().push_list().add(ip::v4(6u)));
    }
  }
  {
    auto row = sut.push_row();
    auto arr_field = row.push_field("arr");
    auto arr = arr_field.push_list();
    {
      auto rec = arr.push_record();
      REQUIRE(not rec.push_field("subnet").add(row_2_1_subnet));
      auto ip_arr_arr_field = rec.push_field("ip arr");
      auto ip_arr_arr = ip_arr_arr_field.push_list();
      auto ip_arr_1 = ip_arr_arr.push_list();
      REQUIRE(not ip_arr_1.add(ip::v4(0xFF << 1)));
      REQUIRE(not ip_arr_1.add(ip::v4(0xFF << 2)));
      auto ip_arr_2 = ip_arr_arr.push_list();
      REQUIRE(not ip_arr_2.add(ip::v4(0xFF << 3)));
      REQUIRE(not ip_arr_2.add(ip::v4(0xFF << 4)));
    }
    {
      auto rec = arr.push_record();
      REQUIRE(not rec.push_field("subnet").add(row_2_2_subnet));
      REQUIRE(not rec.push_field("ip arr").push_list().push_list().add(
        ip::v4(0xFF << 5)));
    }
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 2u);
  CHECK_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))),
              (list{record{{"subnet", row_1_1_subnet},
                           {"ip arr", list{list{ip::v4(2u), ip::v4(3u)},
                                           list{ip::v4(4u)}}}},
                    {record{
                      {"subnet", row_1_2_subnet},
                      {"ip arr", list{{list{ip::v4(6u)}}}},
                    }}}));

  CHECK_EQUAL(
    (materialize(out.at(1u, 0u))),
    (list{record{{"subnet", row_2_1_subnet},
                 {"ip arr", list{list{ip::v4(0xFF << 1), ip::v4(0xFF << 2)},
                                 list{ip::v4(0xFF << 3), ip::v4(0xFF << 4)}}}},
          {record{
            {"subnet", row_2_2_subnet},
            {"ip arr", list{{list{ip::v4(0xFF << 5)}}}},
          }}}));
  const auto schema = tenzir::type{
    record_type{{"arr", list_type{record_type{
                          {"subnet", subnet_type{}},
                          {"ip arr", list_type{type{list_type{ip_type{}}}}},
                        }}}}};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(two rows with array) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{5}));
    auto arrow_field = row.push_field("arr");
    auto arr = arrow_field.push_list();
    REQUIRE(not arr.add(int64_t{1}));
    REQUIRE(not arr.add(int64_t{2}));
  }
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{10}));
    REQUIRE(not row.push_field("arr").push_list().add(int64_t{3}));
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 2u);
  CHECK_EQUAL(out.columns(), 2u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), int64_t{5});
  CHECK_EQUAL((materialize(out.at(1u, 0u))), int64_t{10});
  CHECK_EQUAL((materialize(out.at(0u, 1u))), (list{int64_t{1}, int64_t{2}}));
  CHECK_EQUAL((materialize(out.at(1u, 1u))), (list{int64_t{3}}));
  const auto schema = tenzir::type{record_type{
    {"int", int64_type{}},
    {"arr", list_type{int64_type{}}},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(new fields added in new rows) {
  adaptive_table_slice_builder sut;
  REQUIRE(not sut.push_row().push_field("int").add(int64_t{5}));
  {
    auto row = sut.push_row();
    auto arr_field = row.push_field("arr");
    auto arr = arr_field.push_list();
    REQUIRE(not arr.push_list().add(int64_t{3}));
    REQUIRE(not arr.push_list().add(int64_t{4}));
  }
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{1}));
    REQUIRE(not row.push_field("str").add("strr"));
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 3u);
  CHECK_EQUAL(out.columns(), 3u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), int64_t{5});
  CHECK_EQUAL((materialize(out.at(1u, 0u))), caf::none);
  CHECK_EQUAL((materialize(out.at(2u, 0u))), int64_t{1});

  CHECK_EQUAL((materialize(out.at(0u, 1u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 1u))),
              (list{list{int64_t{3}}, list{int64_t{4}}}));
  CHECK_EQUAL((materialize(out.at(2u, 1u))), caf::none);

  CHECK_EQUAL((materialize(out.at(0u, 2u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 2u))), caf::none);
  CHECK_EQUAL((materialize(out.at(2u, 2u))), "strr");

  const auto schema = tenzir::type{record_type{
    {"int", int64_type{}},
    {"arr", list_type{type{list_type{int64_type{}}}}},
    {"str", string_type{}},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(single empty struct field results in empty table slice) {
  adaptive_table_slice_builder sut;
  sut.push_row().push_field("struct").push_record();
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 0u);
  CHECK_EQUAL(out.columns(), 0u);
}

TEST(empty struct is not added to the output table slice) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    row.push_field("struct").push_record();
    REQUIRE(not row.push_field("int").add(int64_t{2312}));
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 1u);
  CHECK_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), int64_t{2312});
  const auto schema = tenzir::type{record_type{
    {"int", int64_type{}},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(single empty array field results in empty table slice) {
  adaptive_table_slice_builder sut;
  sut.push_row().push_field("arr").push_list();
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 0u);
  CHECK_EQUAL(out.columns(), 0u);
}

TEST(empty array is not added to the output table slice) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    row.push_field("arr").push_list();
    REQUIRE(not row.push_field("int").add(int64_t{2312}));
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 1u);
  CHECK_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), int64_t{2312});
  const auto schema = tenzir::type{record_type{
    {"int", int64_type{}},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(
  empty structs and arrays fields change into non empty ones in the next rows) {
  adaptive_table_slice_builder sut;
  REQUIRE(not sut.push_row().push_field("int").add(int64_t{5}));
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{10}));
    row.push_field("arr").push_list();
  }
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{15}));
    row.push_field("struct").push_record();
  }
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{20}));
    REQUIRE(not row.push_field("arr").push_list().add("arr1"));
  }
  REQUIRE(not sut.push_row().push_field("int").add(int64_t{25}));
  REQUIRE(not sut.push_row()
                .push_field("struct")
                .push_record()
                .push_field("struct.str")
                .add("str"));
  {
    auto row = sut.push_row();
    auto root_struct = row.push_field("struct").push_record();
    REQUIRE(not root_struct.push_field("struct.str").add("str2"));
    auto inner_struct_field = root_struct.push_field("struct.struct");
    auto inner_struct = inner_struct_field.push_record();
    REQUIRE(not inner_struct.push_field("struct.struct.int").add(int64_t{90}));
    auto arr_field = inner_struct.push_field("struct.struct.array");
    auto arr = arr_field.push_list();
    REQUIRE(not arr.add(int64_t{10}));
    REQUIRE(not arr.add(int64_t{20}));
  }

  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 7u);
  CHECK_EQUAL(out.columns(), 5u);

  CHECK_EQUAL((materialize(out.at(0u, 0u))), int64_t{5});
  CHECK_EQUAL((materialize(out.at(1u, 0u))), int64_t{10});
  CHECK_EQUAL((materialize(out.at(2u, 0u))), int64_t{15});
  CHECK_EQUAL((materialize(out.at(3u, 0u))), int64_t{20});
  CHECK_EQUAL((materialize(out.at(4u, 0u))), int64_t{25});
  CHECK_EQUAL((materialize(out.at(5u, 0u))), caf::none);
  CHECK_EQUAL((materialize(out.at(6u, 0u))), caf::none);

  CHECK_EQUAL((materialize(out.at(0u, 1u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 1u))), caf::none);
  CHECK_EQUAL((materialize(out.at(2u, 1u))), caf::none);
  CHECK_EQUAL((materialize(out.at(3u, 1u))), list{"arr1"});
  CHECK_EQUAL((materialize(out.at(4u, 1u))), caf::none);
  CHECK_EQUAL((materialize(out.at(5u, 1u))), caf::none);
  CHECK_EQUAL((materialize(out.at(6u, 1u))), caf::none);

  CHECK_EQUAL((materialize(out.at(0u, 2u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 2u))), caf::none);
  CHECK_EQUAL((materialize(out.at(2u, 2u))), caf::none);
  CHECK_EQUAL((materialize(out.at(3u, 2u))), caf::none);
  CHECK_EQUAL((materialize(out.at(4u, 2u))), caf::none);
  CHECK_EQUAL((materialize(out.at(5u, 2u))), "str");
  CHECK_EQUAL((materialize(out.at(6u, 2u))), "str2");

  CHECK_EQUAL((materialize(out.at(0u, 3u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 3u))), caf::none);
  CHECK_EQUAL((materialize(out.at(2u, 3u))), caf::none);
  CHECK_EQUAL((materialize(out.at(3u, 3u))), caf::none);
  CHECK_EQUAL((materialize(out.at(4u, 3u))), caf::none);
  CHECK_EQUAL((materialize(out.at(5u, 3u))), caf::none);
  CHECK_EQUAL((materialize(out.at(6u, 3u))), int64_t{90});

  CHECK_EQUAL((materialize(out.at(0u, 4u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 4u))), caf::none);
  CHECK_EQUAL((materialize(out.at(2u, 4u))), caf::none);
  CHECK_EQUAL((materialize(out.at(3u, 4u))), caf::none);
  CHECK_EQUAL((materialize(out.at(4u, 4u))), caf::none);
  CHECK_EQUAL((materialize(out.at(5u, 4u))), caf::none);
  CHECK_EQUAL((materialize(out.at(6u, 4u))), (list{int64_t{10}, int64_t{20}}));

  const auto schema = tenzir::type{record_type{
    {"int", int64_type{}},
    {"arr", list_type{string_type{}}},
    {"struct",
     record_type{
       {"struct.str", string_type{}},
       {"struct.struct",
        record_type{
          {"struct.struct.int", int64_type{}},
          {"struct.struct.array", list_type{int64_type{}}},
        }},
     }},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(append nulls to the first field of a record field when a different field
       was added on the second row and the first field didnt have value added) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto record_field = row.push_field("record");
    REQUIRE(
      not record_field.push_record().push_field("field1").add(int64_t{1}));
  }
  {
    auto row = sut.push_row();
    auto record_field = row.push_field("record");
    REQUIRE(
      not record_field.push_record().push_field("field2").add("field2 val"));
  }

  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), int64_t{1});
  CHECK_EQUAL((materialize(out.at(0u, 1u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 0u))), caf::none);
  CHECK_EQUAL((materialize(out.at(1u, 1u))), "field2 val");
}

TEST(field not present after removing the row which introduced the field) {
  adaptive_table_slice_builder sut;
  REQUIRE(not sut.push_row().push_field("int").add(int64_t{5}));
  auto row = sut.push_row();
  REQUIRE(not row.push_field("int").add(int64_t{10}));
  REQUIRE(not row.push_field("str").add("str"));
  row.cancel();
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 1u);
  REQUIRE_EQUAL(output.columns(), 1u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), int64_t{5});
}

TEST(remove basic row) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto rec = row.push_field("record").push_record();
    REQUIRE(not rec.push_field("rec int").add(int64_t{1}));
    REQUIRE(not rec.push_field("rec str").add("str"));
  }
  auto row = sut.push_row();
  {
    auto rec = row.push_field("record").push_record();
    REQUIRE(not rec.push_field("rec int").add(int64_t{2}));
    REQUIRE(not rec.push_field("rec str").add("str2"));
  }
  row.cancel();
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 1u);
  REQUIRE_EQUAL(output.columns(), 2u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), int64_t{1});
  CHECK_EQUAL(materialize(output.at(0u, 1u)), "str");
}

TEST(remove row list) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    REQUIRE(not list.add(int64_t{1}));
    REQUIRE(not list.add(int64_t{2}));
  }

  auto row = sut.push_row();
  {
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    REQUIRE(not list.add(int64_t{3}));
    REQUIRE(not list.add(int64_t{4}));
  }
  row.cancel();
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 1u);
  REQUIRE_EQUAL(output.columns(), 1u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), (list{int64_t{1}, int64_t{2}}));
}

TEST(remove row list of records) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    auto record = list.push_record();
    REQUIRE(not record.push_field("list_rec_int").add(int64_t{1}));
  }

  auto row = sut.push_row();
  {
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    auto record = list.push_record();
    REQUIRE(not record.push_field("list_rec_int").add(int64_t{2}));
  }
  row.cancel();

  {
    auto row = sut.push_row();
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    auto record = list.push_record();
    REQUIRE(not record.push_field("list_rec_int").add(int64_t{3}));
  }
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 2u);
  REQUIRE_EQUAL(output.columns(), 1u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), (list{record{
                                                {"list_rec_int", int64_t{1}},
                                              }}));
  CHECK_EQUAL(materialize(output.at(1u, 0u)), (list{record{
                                                {"list_rec_int", int64_t{3}},
                                              }}));
}

TEST(remove row list of lists) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto outer_list_field = row.push_field("list");
    auto outer_list = outer_list_field.push_list();
    auto inner_list = outer_list.push_list();
    REQUIRE(not inner_list.add(int64_t{1u}));
  }

  auto row = sut.push_row();
  auto outer_list_field = row.push_field("list");
  auto outer_list = outer_list_field.push_list();
  auto inner_list = outer_list.push_list();
  REQUIRE(not inner_list.add(int64_t{2u}));
  REQUIRE(not inner_list.add(int64_t{3u}));
  row.cancel();

  {
    auto row = sut.push_row();
    auto outer_list_field = row.push_field("list");
    auto outer_list = outer_list_field.push_list();
    auto inner_list = outer_list.push_list();
    REQUIRE(not inner_list.add(int64_t{4u}));
    REQUIRE(not inner_list.add(int64_t{5u}));
  }

  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 2u);
  REQUIRE_EQUAL(output.columns(), 1u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), list{{list{int64_t{1}}}});
  CHECK_EQUAL(materialize(output.at(1u, 0u)),
              (list{{list{int64_t{4}, int64_t{5}}}}));
}

TEST(remove row list of records with list fields) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    auto rec = list.push_record();
    REQUIRE(not rec.push_field("int").add(int64_t{1}));
    auto inner_list_field = rec.push_field("inner list");
    auto inner_list = inner_list_field.push_list();
    auto inner_record = inner_list.push_record();
    REQUIRE(not inner_record.push_field("str").add("str1"));
  }

  auto row = sut.push_row();
  {
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    auto rec = list.push_record();
    REQUIRE(not rec.push_field("int").add(int64_t{2}));
    auto inner_list_field = rec.push_field("inner list");
    auto inner_list = inner_list_field.push_list();
    auto inner_record = inner_list.push_record();
    REQUIRE(not inner_record.push_field("str").add("str2"));
  }

  row.cancel();
  {
    auto row = sut.push_row();
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    auto rec = list.push_record();
    REQUIRE(not rec.push_field("int").add(int64_t{3}));
    auto inner_list_field = rec.push_field("inner list");
    auto inner_list = inner_list_field.push_list();
    auto inner_record = inner_list.push_record();
    REQUIRE(not inner_record.push_field("str").add("str3"));
  }
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 2u);
  REQUIRE_EQUAL(output.columns(), 1u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)),
              (list{record{
                {"int", int64_t{1}},
                {"inner list", list{record{
                                 {"str", "str1"},
                               }}},
              }}));
  CHECK_EQUAL(materialize(output.at(1u, 0u)),
              (list{record{
                {"int", int64_t{3}},
                {"inner list", list{record{
                                 {"str", "str3"},
                               }}},
              }}));
}

TEST(remove row with non empty list after pushing empty lists to previous rows) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    row.push_field("list").push_list();
    REQUIRE(not row.push_field("int").add(int64_t{10}));
  }
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{20}));
  }

  auto row = sut.push_row();
  {
    REQUIRE(not row.push_field("list").push_list().add(int64_t{1}));
    REQUIRE(not row.push_field("int").add(int64_t{30}));
  }

  row.cancel();
  REQUIRE(not sut.push_row().push_field("str").add("str0"));
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 3u);
  REQUIRE_EQUAL(output.columns(), 2u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), int64_t{10});
  CHECK_EQUAL(materialize(output.at(0u, 1u)), caf::none);
  CHECK_EQUAL(materialize(output.at(1u, 0u)), int64_t{20});
  CHECK_EQUAL(materialize(output.at(1u, 1u)), caf::none);
  CHECK_EQUAL(materialize(output.at(2u, 0u)), caf::none);
  CHECK_EQUAL(materialize(output.at(2u, 1u)), "str0");
}

TEST(remove row empty list) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    row.push_field("list").push_list();
    REQUIRE(not row.push_field("int").add(int64_t{10}));
  }

  auto row = sut.push_row();
  REQUIRE(not row.push_field("int").add(int64_t{20}));

  row.cancel();
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 1u);
  REQUIRE_EQUAL(output.columns(), 1u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), int64_t{10});
}

TEST(remove row with extension type fields) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("ip1").add(ip::v4(0xFF << 1)));
    REQUIRE(not row.push_field("ip2").add(ip::v4(0xFF << 1)));
  }
  auto row = sut.push_row();
  REQUIRE(not row.push_field("ip1").add(ip::v4(0xFF << 2)));
  REQUIRE(not row.push_field("ip2").add(ip::v4(0xFF << 2)));
  row.cancel();
  auto output = sut.finish();
  REQUIRE_EQUAL(output.rows(), 1u);
  REQUIRE_EQUAL(output.columns(), 2u);
  CHECK_EQUAL(materialize(output.at(0u, 0u)), ip::v4(0xFF << 1));
  CHECK_EQUAL(materialize(output.at(0u, 1u)), ip::v4(0xFF << 1));
}

TEST(Add nulls to fields that didnt have values added when adaptive builder is
       constructed with a schema) {
  const auto schema = tenzir::type{
    "a nice name", record_type{{"int1", int64_type{}},
                               {"str1", string_type{}},
                               {"nested", record_type{
                                            {"rec1", int64_type{}},
                                            {"rec2", string_type{}},
                                          }}}};
  auto sut = adaptive_table_slice_builder{schema};
  REQUIRE(not sut.push_row().push_field("int1").add(int64_t{5238592}));
  auto out = sut.finish(schema.name());
  REQUIRE_EQUAL(schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 4u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), int64_t{5238592});
  CHECK_EQUAL(materialize(out.at(0u, 1u)), caf::none);
  CHECK_EQUAL(materialize(out.at(0u, 2u)), caf::none);
  CHECK_EQUAL(materialize(out.at(0u, 3u)), caf::none);
}

TEST(Allow new fields to be added when adaptive builder is constructed with a
       schema and allow_fields_discovery set to true) {
  const auto starting_schema
    = tenzir::type{"a nice name", record_type{{"int1", int64_type{}}}};

  auto sut = adaptive_table_slice_builder{starting_schema, true};
  REQUIRE(not sut.push_row().push_field("int1").add(int64_t{5238592}));
  REQUIRE(not sut.push_row().push_field("int2").add(int64_t{1}));
  auto out = sut.finish();

  const auto schema = tenzir::type{record_type{
    {"int1", int64_type{}},
    {"int2", int64_type{}},
  }};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), int64_t{5238592});
  CHECK_EQUAL(materialize(out.at(0u, 1u)), caf::none);
  CHECK_EQUAL(materialize(out.at(1u, 0u)), caf::none);
  CHECK_EQUAL(materialize(out.at(1u, 1u)), int64_t{1});
}

TEST(Add enumeration type from a string representation to a basic field) {
  const auto enum_type = enumeration_type{{"enum1"}, {"enum2"}, {"enum3"}};
  const auto starting_schema
    = tenzir::type{"a nice name", record_type{{"enum", enum_type}}};

  auto sut = adaptive_table_slice_builder{starting_schema};
  REQUIRE(not sut.push_row().push_field("enum").add("enum2"));
  auto out = sut.finish(starting_schema.name());
  CHECK_EQUAL(starting_schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)),
              detail::narrow_cast<enumeration>(*enum_type.resolve("enum2")));
}

TEST(Add enumeration type from a string representation to a list of enums) {
  const auto enum_type = enumeration_type{{"enum5"}, {"enum6"}, {"enum7"}};
  const auto starting_schema
    = tenzir::type{"a nice name", record_type{{"list", list_type{enum_type}}}};

  auto sut = adaptive_table_slice_builder{starting_schema};
  {
    auto row = sut.push_row();
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    REQUIRE(not list.add("enum7"));
    REQUIRE(not list.add("enum5"));
  }
  auto out = sut.finish(starting_schema.name());
  CHECK_EQUAL(starting_schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL(
    materialize(out.at(0u, 0u)),
    (list{detail::narrow_cast<enumeration>(*enum_type.resolve("enum7")),
          detail::narrow_cast<enumeration>(*enum_type.resolve("enum5"))}));
}

TEST(Add enumeration type from an enum representation to a basic field) {
  const auto enum_type = enumeration_type{{"enum1"}, {"enum2"}, {"enum3"}};
  const auto starting_schema
    = tenzir::type{"a nice name", record_type{{"enum", enum_type}}};

  auto sut = adaptive_table_slice_builder{starting_schema};
  const auto input
    = detail::narrow_cast<enumeration>(*enum_type.resolve("enum2"));
  REQUIRE(not sut.push_row().push_field("enum").add(input));
  auto out = sut.finish(starting_schema.name());
  CHECK_EQUAL(starting_schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), input);
}

TEST(Add enumeration type from an enum representation to a list of enums) {
  const auto enum_type = enumeration_type{{"enum5"}, {"enum6"}, {"enum7"}};
  const auto starting_schema
    = tenzir::type{record_type{{"list", list_type{enum_type}}}};

  const auto input_1
    = detail::narrow_cast<enumeration>(*enum_type.resolve("enum7"));
  const auto input_2
    = detail::narrow_cast<enumeration>(*enum_type.resolve("enum5"));
  auto sut = adaptive_table_slice_builder{starting_schema};
  {
    auto row = sut.push_row();
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    REQUIRE(not list.add(input_1));
    REQUIRE(not list.add(input_2));
  }
  auto out = sut.finish();
  CHECK_EQUAL((type{starting_schema.make_fingerprint(), starting_schema}),
              out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), (list{input_1, input_2}));
}

TEST(Add none for enumerations that dont exist)
{
  const auto enum_type = enumeration_type{{"enum1"}, {"enum2"}, {"enum3"}};
  const auto starting_schema
    = tenzir::type{"a nice name", record_type{{"enum", enum_type}}};

  auto sut = adaptive_table_slice_builder{starting_schema};
  REQUIRE(not sut.push_row().push_field("enum").add("enum4"));
  auto out = sut.finish(starting_schema.name());
  CHECK_EQUAL(starting_schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), caf::none);
}

TEST(Fixed fields builder can be reused after finish call) {
  const auto schema
    = tenzir::type{"a nice name", record_type{{"int1", int64_type{}},
                                              {"str1", string_type{}}}};
  auto sut = adaptive_table_slice_builder{schema, true};

  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int1").add(int64_t{1}));
    REQUIRE(not row.push_field("str1").add("str"));
  }
  auto out = sut.finish(schema.name());
  REQUIRE_EQUAL(schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), int64_t{1});
  CHECK_EQUAL(materialize(out.at(0u, 1u)), "str");

  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int1").add(int64_t{2}));
    REQUIRE(not row.push_field("str1").add("str2"));
  }
  out = sut.finish(schema.name());
  REQUIRE_EQUAL(schema, out.schema());

  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), int64_t{2});
  CHECK_EQUAL(materialize(out.at(0u, 1u)), "str2");
}

TEST(Fixed fields builder add record type) {
  const auto schema = tenzir::type{
    "a nice name",
    record_type{
      {"record", record_type{
                   {"int", int64_type{}},
                   {"list", list_type{record_type{
                              {"str", string_type{}},
                              {"nested list", list_type{int64_type{}}},
                            }}},
                 }}}};
  auto sut = adaptive_table_slice_builder{schema};
  {
    auto row = sut.push_row();
    auto record_field = row.push_field("record");
    auto record = record_field.push_record();
    REQUIRE(not record.push_field("int").add(int64_t{1}));
    auto list_field = record.push_field("list");
    auto list = list_field.push_list();
    auto list_record = list.push_record();
    REQUIRE(not list_record.push_field("str").add("str1"));
    auto nested_list_field = list_record.push_field("nested list");
    auto nested_list = nested_list_field.push_list();
    REQUIRE(not nested_list.add(int64_t{1}));
    REQUIRE(not nested_list.add(int64_t{2}));
  }
  {
    auto row = sut.push_row();
    auto record_field = row.push_field("record");
    auto record = record_field.push_record();
    REQUIRE(not record.push_field("int").add(int64_t{2}));
    auto list_field = record.push_field("list");
    auto list = list_field.push_list();
    {
      auto list_record = list.push_record();
      REQUIRE(not list_record.push_field("str").add("str2"));
      auto nested_list_field = list_record.push_field("nested list");
      auto nested_list = nested_list_field.push_list();
      REQUIRE(not nested_list.add(int64_t{3}));
      REQUIRE(not nested_list.add(int64_t{4}));
    }
    {
      auto list_record = list.push_record();
      REQUIRE(not list_record.push_field("str").add("str3"));
      auto nested_list_field = list_record.push_field("nested list");
      auto nested_list = nested_list_field.push_list();
      REQUIRE(not nested_list.add(int64_t{100}));
    }
  }
  auto out = sut.finish(schema.name());
  REQUIRE_EQUAL(schema, out.schema());
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), int64_t{1});
  CHECK_EQUAL(materialize(out.at(0u, 1u)),
              (list{
                {record{
                  {"str", "str1"},
                  {"nested list", list{int64_t{1}, int64_t{2}}},
                }},
              }));
  CHECK_EQUAL(materialize(out.at(1u, 0u)), int64_t{2});
  CHECK_EQUAL(materialize(out.at(1u, 1u)),
              (list{
                {record{
                  {"str", "str2"},
                  {"nested list", list{int64_t{3}, int64_t{4}}},
                }},
                {record{
                  {"str", "str3"},
                  {"nested list", list{{int64_t{100}}}},
                }},
              }));
}

TEST(Fixed fields builder remove record type row) {
  const auto schema = tenzir::type{
    "a nice name",
    record_type{
      {"record", record_type{
                   {"int", int64_type{}},
                   {"list", list_type{record_type{
                              {"str", string_type{}},
                              {"nested list", list_type{int64_type{}}},
                            }}},
                 }}}};
  auto sut = adaptive_table_slice_builder{schema};
  auto row_1 = sut.push_row();
  {
    auto record_field = row_1.push_field("record");
    auto record = record_field.push_record();
    REQUIRE(not record.push_field("int").add(int64_t{1}));
    auto list_field = record.push_field("list");
    auto list = list_field.push_list();
    auto list_record = list.push_record();
    REQUIRE(not list_record.push_field("str").add("str1"));
    auto nested_list_field = list_record.push_field("nested list");
    auto nested_list = nested_list_field.push_list();
    REQUIRE(not nested_list.add(int64_t{1}));
    REQUIRE(not nested_list.add(int64_t{2}));
  }
  row_1.cancel();
  {
    auto row = sut.push_row();
    auto record_field = row.push_field("record");
    auto record = record_field.push_record();
    REQUIRE(not record.push_field("int").add(int64_t{2}));
    auto list_field = record.push_field("list");
    auto list = list_field.push_list();
    {
      auto list_record = list.push_record();
      REQUIRE(not list_record.push_field("str").add("str2"));
      auto nested_list_field = list_record.push_field("nested list");
      auto nested_list = nested_list_field.push_list();
      REQUIRE(not nested_list.add(int64_t{3}));
      REQUIRE(not nested_list.add(int64_t{4}));
    }
    {
      auto list_record = list.push_record();
      REQUIRE(not list_record.push_field("str").add("str3"));
      auto nested_list_field = list_record.push_field("nested list");
      auto nested_list = nested_list_field.push_list();
      REQUIRE(not nested_list.add(int64_t{100}));
    }
  }
  auto out = sut.finish(schema.name());
  REQUIRE_EQUAL(schema, out.schema());
  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 2u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), int64_t{2});
  CHECK_EQUAL(materialize(out.at(0u, 1u)),
              (list{
                {record{
                  {"str", "str2"},
                  {"nested list", list{int64_t{3}, int64_t{4}}},
                }},
                {record{
                  {"str", "str3"},
                  {"nested list", list{{int64_t{100}}}},
                }},
              }));
}

TEST(Remove row when not all of the schema fields got value added) {
  const auto schema
    = tenzir::type{"a nice name", record_type{{"int", int64_type{}},
                                              {"str", string_type{}},
                                              {"duration", duration_type{}}}};
  auto sut = adaptive_table_slice_builder{schema};
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{5}));
    row.cancel();
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 0u);
}

TEST(Field type changes when it was first discovered with a different one but
       the value got removed and a different type value was added) {
  auto sut = adaptive_table_slice_builder{};
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{5}));
    row.cancel();
  }
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add("string"));
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 1u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), "string");
}

TEST(Dont add a value of a different type to a fixed type field which had its
       only value removed) {
  const auto schema
    = tenzir::type{"a nice name", record_type{{"int", int64_type{}}}};
  auto sut = adaptive_table_slice_builder{schema};
  {
    auto row = sut.push_row();
    REQUIRE(not row.push_field("int").add(int64_t{5}));
    row.cancel();
  }
  {
    auto row = sut.push_row();
    // this returns an error because the "int" field is fixed to be of
    // int64_type. We can't add a values that represent a different type.
    REQUIRE(row.push_field("int").add("string"));
  }
  auto out = sut.finish();
  CHECK_EQUAL(out.rows(), 0u);
}

TEST(successful cast to list value type) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto list_f = row.push_field("list");
    auto list = list_f.push_list();
    REQUIRE(not list.add(int64_t{1}));
    REQUIRE(not list.add("520"));
    REQUIRE(not list.add(469382.0));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))),
              (list{int64_t{1}, int64_t{520}, int64_t{469382}}));
  const auto schema
    = tenzir::type{record_type{{"list", list_type{int64_type{}}}}};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(cast the whole column to a common type when input type and already existing
       field type cannot be cast) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("a");
    REQUIRE(not f.add(true));
  }
  {
    auto row = sut.push_row();
    auto f = row.push_field("a");
    REQUIRE(not f.add(std::chrono::nanoseconds{20}));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), "true");
  CHECK_EQUAL((materialize(out.at(1u, 0u))), "20.0ns");
  const auto schema = tenzir::type{record_type{{"a", string_type{}}}};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(cast the whole column to a double type when input double is not 0.0
     or 1.0) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("a");
    REQUIRE(not f.add(true));
  }
  {
    auto row = sut.push_row();
    auto f = row.push_field("a");
    REQUIRE(not f.add(20.0));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), 1.0);
  CHECK_EQUAL((materialize(out.at(1u, 0u))), 20.0);
  const auto schema = tenzir::type{record_type{{"a", double_type{}}}};
  const auto expected_schema = tenzir::type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(common type casting of a nested record field) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("a");
    auto rec = f.push_record();
    auto nested = rec.push_field("nested");
    REQUIRE(not nested.add(true));
  }
  {
    auto row = sut.push_row();
    auto f = row.push_field("a");
    auto rec = f.push_record();
    auto nested = rec.push_field("nested");
    REQUIRE(not nested.add("str"));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), "true");
  CHECK_EQUAL((materialize(out.at(1u, 0u))), "str");
  const auto schema
    = type{record_type{{"a", record_type{{"nested", string_type{}}}}}};
  const auto expected_schema = type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(common type casting of a nested record list field) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();

    {
      auto rec = list.push_record();
      auto nested = rec.push_field("nested");
      REQUIRE(not nested.add(true));
    }
    {
      auto rec = list.push_record();
      auto nested = rec.push_field("nested");
      REQUIRE(not nested.add(false));
    }
  }
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    {
      auto rec = list.push_record();
      auto nested = rec.push_field("nested");
      REQUIRE(not nested.add(false));
    }
    {
      auto rec = list.push_record();
      auto nested = rec.push_field("nested");
      REQUIRE(not nested.add("str"));
    }
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))),
              (list{record{{"nested", "true"}}, record{
                                                  {{"nested", "false"}},
                                                }}));
  CHECK_EQUAL((materialize(out.at(1u, 0u))),
              (list{record{{"nested", "false"}}, record{{"nested", "str"}}}));
  const auto schema = type{
    record_type{{"list", list_type{record_type{{"nested", string_type{}}}}}}};
  const auto expected_schema = type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(common type casting of a list<record<record>> field with additional fields
       in both records) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    {
      auto rec = list.push_record();
      auto int_field = rec.push_field("int");
      REQUIRE(not int_field.add(int64_t{5}));
      {
        auto nested = rec.push_field("nested rec");
        auto nested_rec = nested.push_record();
        auto nested_int_field = nested_rec.push_field("nested int");
        REQUIRE(not nested_int_field.add(int64_t{50}));
        auto cast_field = nested_rec.push_field("cast");
        REQUIRE(not cast_field.add(int64_t{500}));
        auto nested_str_field = nested_rec.push_field("nested str");
        REQUIRE(not nested_str_field.add("nested_str1"));
      }
      auto str_field = rec.push_field("str");
      REQUIRE(not str_field.add("str1"));
    }
    {
      auto rec = list.push_record();
      auto int_field = rec.push_field("int");
      REQUIRE(not int_field.add(int64_t{2}));
      {
        auto nested = rec.push_field("nested rec");
        auto nested_rec = nested.push_record();
        auto nested_int_field = nested_rec.push_field("nested int");
        REQUIRE(not nested_int_field.add(int64_t{20}));
        auto cast_field = nested_rec.push_field("cast");
        REQUIRE(not cast_field.add(int64_t{200}));
        auto nested_str_field = nested_rec.push_field("nested str");
        REQUIRE(not nested_str_field.add("nested_str2"));
      }
      auto str_field = rec.push_field("str");
      REQUIRE(not str_field.add("str2"));
    }
  }
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    {
      auto rec = list.push_record();
      auto int_field = rec.push_field("int");
      REQUIRE(not int_field.add(int64_t{3}));
      {
        auto nested = rec.push_field("nested rec");
        auto nested_rec = nested.push_record();
        auto nested_int_field = nested_rec.push_field("nested int");
        REQUIRE(not nested_int_field.add(int64_t{30}));
        auto cast_field = nested_rec.push_field("cast");
        REQUIRE(not cast_field.add("cast_str"));
        auto nested_str_field = nested_rec.push_field("nested str");
        REQUIRE(not nested_str_field.add("nested_str3"));
      }
      auto str_field = rec.push_field("str");
      REQUIRE(not str_field.add("str3"));
    }
    {
      auto rec = list.push_record();
      auto int_field = rec.push_field("int");
      REQUIRE(not int_field.add(int64_t{4}));
      {
        auto nested = rec.push_field("nested rec");
        auto nested_rec = nested.push_record();
        auto nested_int_field = nested_rec.push_field("nested int");
        REQUIRE(not nested_int_field.add(int64_t{40}));
        auto cast_field = nested_rec.push_field("cast");
        // int type should be casted to a new string_type.
        REQUIRE(not cast_field.add(int64_t{400}));
        auto nested_str_field = nested_rec.push_field("nested str");
        REQUIRE(not nested_str_field.add("nested_str4"));
      }
      auto str_field = rec.push_field("str");
      REQUIRE(not str_field.add("str4"));
    }
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))),
              (list{record{
                      {"int", int64_t{5}},
                      {"nested rec",
                       record{
                         {"nested int", int64_t{50}},
                         {"cast", "+500"},
                         {"nested str", "nested_str1"},
                       }},
                      {"str", "str1"},
                    },
                    record{
                      {"int", int64_t{2}},
                      {"nested rec",
                       record{
                         {"nested int", int64_t{20}},
                         {"cast", "+200"},
                         {"nested str", "nested_str2"},
                       }},
                      {"str", "str2"},
                    }}));
  CHECK_EQUAL((materialize(out.at(1u, 0u))),
              (list{record{
                      {"int", int64_t{3}},
                      {"nested rec",
                       record{
                         {"nested int", int64_t{30}},
                         {"cast", "cast_str"},
                         {"nested str", "nested_str3"},
                       }},
                      {"str", "str3"},
                    },
                    record{
                      {"int", int64_t{4}},
                      {"nested rec",
                       record{
                         {"nested int", int64_t{40}},
                         {"cast", "+400"},
                         {"nested str", "nested_str4"},
                       }},
                      {"str", "str4"},
                    }}));
  const auto schema
    = type{record_type{{"list", list_type{record_type{
                                  {"int", int64_type{}},
                                  {"nested rec",
                                   record_type{
                                     {"nested int", int64_type{}},
                                     {"cast", string_type{}},
                                     {"nested str", string_type{}},
                                   }},
                                  {"str", string_type{}},

                                }}}}};
  const auto expected_schema = type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(field changes in the deepest record in list<record<list<record>>>and
       another field changes in the first record) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    {
      auto rec = list.push_record();
      {
        auto nested_list_field = rec.push_field("nested list");
        auto nested_list = nested_list_field.push_list();
        auto nested_record = nested_list.push_record();
        auto cast_field1 = nested_record.push_field("cast_field1");
        REQUIRE(not cast_field1.add(true));
      }
      auto cast_field2 = rec.push_field("cast_field2");
      REQUIRE(not cast_field2.add(true));
    }
  }
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    {
      auto rec = list.push_record();
      {
        auto nested_list_field = rec.push_field("nested list");
        auto nested_list = nested_list_field.push_list();
        auto nested_record = nested_list.push_record();
        auto cast_field1 = nested_record.push_field("cast_field1");
        REQUIRE(not cast_field1.add("cast_field1_str"));
      }
      auto cast_field2 = rec.push_field("cast_field2");
      REQUIRE(not cast_field2.add(10.0));
    }
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))),
              (list{{
                record{
                  {"nested list", list{{record{
                                    {"cast_field1", "true"},
                                  }}}},
                  {"cast_field2", 1.0},
                },
              }}));
  CHECK_EQUAL((materialize(out.at(1u, 0u))),
              (list{{
                record{
                  {"nested list", list{{record{
                                    {"cast_field1", "cast_field1_str"},
                                  }}}},
                  {"cast_field2", 10.0},
                },
              }}));

  const auto schema = type{
    record_type{{"list", list_type{record_type{
                           {"nested list", list_type{record_type{
                                             {"cast_field1", string_type{}},
                                           }}},
                           {"cast_field2", double_type{}},

                         }}}}};
  const auto expected_schema = type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(cast a list<ip> into list<string> when the list field is not a child of
       another list type) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    REQUIRE(not list.add(ip::v4(0xFF)));
    REQUIRE(not list.add(ip::v4(0xFF << 1)));
    REQUIRE(not list.add("str"));
  }
  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))),
              (list{"0.0.0.255", "0.0.1.254", "str"}));
  const auto schema = type{record_type{{"list", list_type{string_type{}}}}};
  const auto expected_schema = type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(list field changes in list<record<list<..>>> with some fields common type
       casting before the list common type cast) {
  adaptive_table_slice_builder sut;
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    {
      auto rec_f = list.push_record();
      {
        auto a = rec_f.push_field("a");
        REQUIRE(not a.add(true));
        auto b = rec_f.push_field("b");
        auto b_list = b.push_list();
        REQUIRE(not b_list.add(false));
        auto c = rec_f.push_field("c");
        REQUIRE(not c.add(false));
      }
    }
  }
  {
    auto row = sut.push_row();
    auto f = row.push_field("list");
    auto list = f.push_list();
    {
      auto rec_f = list.push_record();
      {
        auto a = rec_f.push_field("a");
        REQUIRE(not a.add("a cast"));
        auto b = rec_f.push_field("b");
        auto b_list = b.push_list();
        REQUIRE(not b_list.add(true));
        auto c = rec_f.push_field("c");
        REQUIRE(not c.add(true));
      }
    }
    {
      auto rec_f = list.push_record();
      auto a = rec_f.push_field("a");
      REQUIRE(not a.add(false));
      auto b = rec_f.push_field("b");
      auto b_list = b.push_list();
      REQUIRE(not b_list.add(true));
      REQUIRE(not b_list.add("list cast"));
      REQUIRE(not b_list.add(false));
      auto c = rec_f.push_field("c");
      REQUIRE(not c.add("c cast"));
    }
  }

  auto out = sut.finish();
  REQUIRE_EQUAL(out.rows(), 2u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL((materialize(out.at(0u, 0u))), (list{{
                                               record{
                                                 {"a", "true"},
                                                 {"b", list{{"false"}}},
                                                 {"c", "false"},
                                               },
                                             }}));
  CHECK_EQUAL((materialize(out.at(1u, 0u))), (list{
                                               record{
                                                 {"a", "a cast"},
                                                 {"b", list{{"true"}}},
                                                 {"c", "true"},
                                               },
                                               record{
                                                 {"a", "false"},
                                                 {"b",
                                                  list{
                                                    "true",
                                                    "list cast",
                                                    "false",
                                                  }},
                                                 {"c", "c cast"},
                                               },
                                             }));
  const auto schema = type{record_type{{"list", list_type{record_type{
                                                  {
                                                    "a",
                                                    string_type{},
                                                  },
                                                  {
                                                    "b",
                                                    list_type{string_type{}},
                                                  },
                                                  {
                                                    "c",
                                                    string_type{},
                                                  },
                                                }}}}};
  const auto expected_schema = type{schema.make_fingerprint(), schema};
  CHECK_EQUAL(expected_schema, out.schema());
}

TEST(fixed fields builder will return an error when trying to add a value
       that requires common type cast to the record field) {
  const auto schema = tenzir::type{
    "a nice name",
    record_type{{"a", bool_type{}}},
  };
  auto sut = adaptive_table_slice_builder{schema};
  auto row = sut.push_row();
  {
    auto field = row.push_field("a");
    REQUIRE(not field.add(true));
    REQUIRE(field.add("str"));
  }
  auto out = sut.finish(schema.name());
  REQUIRE_EQUAL(schema, out.schema());
  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), true);
}

TEST(fixed fields builder will return an error when trying to add a value
       that requires common type cast to the list) {
  const auto schema
    = tenzir::type{"a nice name", record_type{
                                    {"list", list_type{bool_type{}}},
                                  }};
  auto sut = adaptive_table_slice_builder{schema};
  auto row = sut.push_row();
  {
    auto list_field = row.push_field("list");
    auto list = list_field.push_list();
    REQUIRE(not list.add(true));
    REQUIRE(list.add("str"));
  }
  auto out = sut.finish(schema.name());
  REQUIRE_EQUAL(schema, out.schema());
  REQUIRE_EQUAL(out.rows(), 1u);
  REQUIRE_EQUAL(out.columns(), 1u);
  CHECK_EQUAL(materialize(out.at(0u, 0u)), (list{{true}}));
}
