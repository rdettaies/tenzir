//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2023 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/cast.hpp"
#include "vast/detail/stable_map.hpp"
#include "vast/table_slice_builder.hpp"
#include "vast/type.hpp"

#include <functional>

namespace vast::detail {

template <concrete_type T>
class concrete_series_builder;

template <class T>
struct make_concrete_series_builder {
  using type = concrete_series_builder<T>;
};

struct series_builder;

// Arrow uses int64_t for all lengths.
using arrow_length_type = int64_t;

class builder_provider {
public:
  using builder_provider_impl = std::function<series_builder&()>;

  explicit(false) builder_provider(
    std::variant<builder_provider_impl, std::reference_wrapper<series_builder>>
      data)
    : data_{std::move(data)} {
  }

  auto provide() -> series_builder&;
  auto type() const -> type;
  auto is_builder_constructed() const -> bool;

private:
  std::variant<builder_provider_impl, std::reference_wrapper<series_builder>>
    data_;
};

// Builder that represents a column of an unknown type.
class unknown_type_builder {
public:
  unknown_type_builder() = default;
  explicit unknown_type_builder(arrow_length_type null_count)
    : null_count_{null_count} {
  }

  arrow_length_type length() const {
    return null_count_;
  }

  auto type() const -> type {
    return {};
  }

  auto add_up_to_n_nulls(arrow_length_type max_null_count) -> void {
    null_count_ = std::max(null_count_, max_null_count);
  }

  auto get_arrow_builder() const -> std::shared_ptr<arrow::ArrayBuilder> {
    return nullptr;
  }

  auto remove_last_row() -> void {
    // remove_last_row can't be practically called when null_count is zero. No
    // need to guard against underflow
    --null_count_;
  }

private:
  arrow_length_type null_count_ = 0u;
};

template <concrete_type Type>
class concrete_series_builder_base {
public:
  explicit concrete_series_builder_base(vast::type type = vast::type{Type{}})
    : type_{std::move(type)},
      builder_(caf::get<Type>(type_).make_arrow_builder(
        arrow::default_memory_pool())) {
  }

  auto add(view<type_to_data_t<Type>> view) {
    const auto s = append_builder(caf::get<Type>(type_), *builder_, view);
    VAST_ASSERT(s.ok());
  }

  auto finish() {
    return builder_->Finish().ValueOrDie();
  }

  auto get_arrow_builder() {
    return builder_;
  }

  auto type() const -> const vast::type& {
    return type_;
  }

  auto length() const -> arrow_length_type {
    return builder_->length();
  }

  auto add_up_to_n_nulls(arrow_length_type max_null_count) -> void {
    VAST_ASSERT(max_null_count >= length());
    const auto status = builder_->AppendNulls(max_null_count - length());
    VAST_ASSERT(status.ok());
  }

  auto remove_last_row() -> bool {
    auto array = builder_->Finish().ValueOrDie();
    auto new_array = array->Slice(0, array->length() - 1);
    const auto status
      = builder_->AppendArraySlice(*new_array->data(), 0u, new_array->length());
    VAST_ASSERT(status.ok());
    return builder_->null_count() == builder_->length();
  }

protected:
  vast::type type_;
  std::shared_ptr<type_to_arrow_builder_t<Type>> builder_;
};

template <concrete_type Type>
class concrete_series_builder : public concrete_series_builder_base<Type> {
public:
  using concrete_series_builder_base<Type>::concrete_series_builder_base;
};

template <>
class concrete_series_builder<enumeration_type>
  : public concrete_series_builder_base<enumeration_type> {
public:
  using concrete_series_builder_base<
    enumeration_type>::concrete_series_builder_base;

  auto add(std::string_view string_value) -> void {
    if (auto resolved
        = caf::get<enumeration_type>(type_).resolve(string_value)) {
      const auto s = append_builder(caf::get<enumeration_type>(type_),
                                    *builder_, *resolved);
      VAST_ASSERT(s.ok());
      return;
    }
    const auto s = builder_->AppendNull();
    VAST_ASSERT(s.ok());
  }

  using concrete_series_builder_base<enumeration_type>::add;
};

class record_series_builder_base {
public:
  auto fill_nulls() -> void;
  auto add_up_to_n_nulls(arrow_length_type max_null_count) -> void;
  auto length() const -> arrow_length_type;
  auto finish() -> std::shared_ptr<arrow::Array>;
  auto remove_last_row() -> void;
  auto append() -> void;

protected:
  auto get_arrow_builder(const type& type)
    -> std::shared_ptr<arrow::StructBuilder>;
  detail::stable_map<std::string, std::unique_ptr<series_builder>>
    field_builders_;
  std::shared_ptr<arrow::StructBuilder> arrow_builder_;
};

template <>
class concrete_series_builder<record_type> : public record_series_builder_base {
public:
  using record_series_builder_base::record_series_builder_base;
  explicit concrete_series_builder(const record_type& type);

  auto get_field_builder_provider(std::string_view field,
                                  arrow_length_type starting_fields_length)
    -> builder_provider;

  auto get_arrow_builder() -> std::shared_ptr<arrow::StructBuilder>;
  auto type() const -> vast::type;
};

class fixed_fields_record_builder : public record_series_builder_base {
public:
  explicit fixed_fields_record_builder(record_type type);

  auto get_field_builder_provider(std::string_view field_name)
    -> builder_provider;
  auto get_arrow_builder() -> std::shared_ptr<arrow::StructBuilder>;
  auto type() const -> const vast::type&;

private:
  vast::type type_;
};

template <>
class concrete_series_builder<list_type> {
public:
  explicit concrete_series_builder(arrow_length_type nulls_to_prepend = 0u);
  explicit concrete_series_builder(const list_type& type,
                                   bool are_fields_fixed);

  auto finish() -> std::shared_ptr<arrow::Array>;
  auto length() const -> arrow_length_type;
  auto type() const -> const vast::type&;
  auto add_up_to_n_nulls(arrow_length_type max_null_count) -> void;

  auto create_builder(const vast::type& value_type) -> void;

  template <class TypeToCastTo = arrow::ArrayBuilder>
  auto get_child_builder(const vast::type& t) -> TypeToCastTo& {
    VAST_ASSERT(child_builders_.contains(t));
    return static_cast<TypeToCastTo&>(*child_builders_[t]);
  }

  auto get_arrow_builder()
    -> std::shared_ptr<type_to_arrow_builder_t<list_type>>;

  // Only one record builder exists in list of records as the deeper nestings
  // are handled by the record builder itself.
  auto get_record_builder() -> series_builder&;
  auto remove_last_row() -> bool;

private:
  auto create_builder_impl(const vast::type& t)
    -> std::shared_ptr<arrow::ArrayBuilder>;

  std::shared_ptr<type_to_arrow_builder_t<list_type>> builder_;
  detail::stable_map<vast::type, arrow::ArrayBuilder*> child_builders_;
  std::unique_ptr<series_builder> record_builder_;
  arrow_length_type nulls_to_prepend_ = 0u;
  bool are_fields_fixed_ = false;
  vast::type type_;
};

using series_builder_base = caf::detail::tl_apply_t<
  caf::detail::tl_push_front_t<
    caf::detail::tl_concat_t<
      caf::detail::tl_map_t<concrete_types, make_concrete_series_builder>,
      caf::detail::type_list<fixed_fields_record_builder>>,
    unknown_type_builder>,
  std::variant>;

struct series_builder : series_builder_base {
  using variant::variant;

  series_builder(const vast::type&, bool are_fields_fixed = false);

  arrow_length_type length() const;

  std::shared_ptr<arrow::ArrayBuilder> get_arrow_builder();

  vast::type type() const;

  template <concrete_type Type>
  auto add(auto view) -> caf::error {
    if constexpr (std::is_same_v<Type, string_type>) {
      if (auto enum_builder
          = std::get_if<concrete_series_builder<enumeration_type>>(this)) {
        enum_builder->add(view);
        return {};
      }
    }
    return add_impl<Type>(std::move(view));
  }

  std::shared_ptr<arrow::Array> finish();

  auto add_up_to_n_nulls(arrow_length_type max_null_count) -> void;
  auto remove_last_row() -> void;

private:
  template <concrete_type ViewType>
  auto
  cast_to_duration(const ViewType& view_type,
                   concrete_series_builder<duration_type>& builder, auto view) {
    auto unit = builder.type().attribute("unit").value_or("s");
    auto result = cast_value(view_type, view, duration_type{}, unit);
    if constexpr (std::is_same_v<decltype(result), caf::expected<void>>) {
      return std::move(result.error());
    } else {
      if (result) {
        builder.add(*result);
        return caf::error{};
      }
      return std::move(result.error());
    }
  }

  auto
  cast_to_duration(const string_type& t,
                   concrete_series_builder<duration_type>& builder, auto view) {
    auto result = cast_value(t, view, duration_type{});
    // It is simpler to just try casting again with unit appended instead of
    // validating if the unit is present.
    if (not result) {
      const auto& type = builder.type();
      auto unit = type.attribute("unit").value_or("s");
      result = cast_value(t, std::string{view} + std::string{unit},
                          caf::get<duration_type>(type));
    }
    if (not result)
      return std::move(result.error());
    builder.add(*result);
    return caf::error{};
  }

  template <concrete_type BuilderType, concrete_type ViewType>
  auto cast_impl(auto& builder, auto view) {
    if constexpr (std::is_same_v<BuilderType, duration_type>) {
      return cast_to_duration(ViewType{}, builder, view);
    } else {
      auto maybe_new_value = cast_value(ViewType{}, view, BuilderType{});
      if (not maybe_new_value)
        return std::move(maybe_new_value.error());
      if constexpr (not std::is_same_v<decltype(maybe_new_value),
                                       caf::expected<void>>) {
        builder.add(*maybe_new_value);
      }
      return caf::error{};
    }
  }

  template <concrete_type Type>
  auto add_impl(auto view) -> caf::error {
    return std::visit(
      detail::overload{
        [view](concrete_series_builder<Type>& same_type_builder) {
          same_type_builder.add(view);
          return caf::error{};
        },
        [this, view](unknown_type_builder& builder) {
          const auto nulls_to_prepend = builder.length();
          auto new_builder = concrete_series_builder<Type>{};
          new_builder.add_up_to_n_nulls(nulls_to_prepend);
          new_builder.add(view);
          *this = std::move(new_builder);
          return caf::error{};
        },
        [view, this]<class BuilderValueType>(
          concrete_series_builder<BuilderValueType>& builder) {
          if (auto maybe_err = can_cast(Type{}, BuilderValueType{});
              not maybe_err) {
            return maybe_err.error();
          }
          auto err = cast_impl<BuilderValueType, Type>(builder, view);
          if (not err)
            return caf::error{};
          if (err and not can_change_builder_type_)
            return err;
          // TODO: implement casting whole array instead of returning error here
          // in upcomming PR.
          return err;
        },
        [view](concrete_series_builder<record_type>&) {
          return caf::make_error(ec::convert_error,
                                 fmt::format("casting to a record is not "
                                             "implemented: ignoring value: {}",
                                             data{materialize(view)}));
        },
        [view](concrete_series_builder<map_type>&) {
          return caf::make_error(ec::convert_error,
                                 fmt::format("casting to a map is not "
                                             "implemented: ignoring value: {}",
                                             data{materialize(view)}));
        },
        [view](concrete_series_builder<list_type>&) {
          return caf::make_error(ec::convert_error,
                                 fmt::format("casting to a list is not "
                                             "implemented: ignoring value: {}",
                                             data{materialize(view)}));
        },
        [view](auto&) {
          return caf::make_error(ec::convert_error,
                                 fmt::format("cast not implemented: ignoring "
                                             "value: {}",
                                             data{materialize(view)}));
        },
      },
      *this);
  }

  bool can_change_builder_type_ = true;
};

} // namespace vast::detail
