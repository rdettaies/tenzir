//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "tenzir/concept/printable/core/operators.hpp"
#include "tenzir/concept/printable/core/printer.hpp"
#include "tenzir/concept/printable/tenzir/json_printer_options.hpp"
#include "tenzir/concept/printable/to_string.hpp"
#include "tenzir/data.hpp"
#include "tenzir/detail/string.hpp"
#include "tenzir/view.hpp"

#include <fmt/format.h>

#include <optional>

namespace tenzir {

struct json_printer : printer_base<json_printer> {
  explicit json_printer(json_printer_options options) noexcept
    : printer_base(), options_{options} {
    // nop
  }

  template <class Iterator>
  struct print_visitor {
    print_visitor(Iterator& out, const json_printer_options& options) noexcept
      : out_{out}, options_{options} {
      // nop
    }

    auto operator()(caf::none_t) noexcept -> bool {
      out_ = fmt::format_to(out_, options_.style.null_, "null");
      return true;
    }

    auto operator()(view<bool> x) noexcept -> bool {
      out_ = x ? fmt::format_to(out_, options_.style.true_, "true")
               : fmt::format_to(out_, options_.style.false_, "false");
      return true;
    }

    auto operator()(view<int64_t> x) noexcept -> bool {
      out_ = fmt::format_to(out_, options_.style.number, "{}", x);
      return true;
    }

    auto operator()(view<uint64_t> x) noexcept -> bool {
      out_ = fmt::format_to(out_, options_.style.number, "{}", x);
      return true;
    }

    auto operator()(view<double> x) noexcept -> bool {
      if (double i; std::modf(x, &i) == 0.0) // NOLINT
        out_ = fmt::format_to(out_, options_.style.number, "{}.0", i);
      else
        out_ = fmt::format_to(out_, options_.style.number, "{}", x);
      return true;
    }

    auto operator()(view<duration> x) noexcept -> bool {
      if (options_.numeric_durations) {
        const auto seconds
          = std::chrono::duration_cast<std::chrono::duration<double>>(x).count();
        return (*this)(seconds);
      }
      out_
        = fmt::format_to(out_, options_.style.string, "\"{}\"", to_string(x));
      return true;
    }

    auto operator()(view<time> x) noexcept -> bool {
      out_
        = fmt::format_to(out_, options_.style.string, "\"{}\"", to_string(x));
      return true;
    }

    auto operator()(view<std::string> x) noexcept -> bool {
      out_ = fmt::format_to(out_, options_.style.string, "{}",
                            detail::json_escape(x));
      return true;
    }

    auto operator()(view<pattern> x) noexcept -> bool {
      return (*this)(x.string());
    }

    auto operator()(view<ip> x) noexcept -> bool {
      out_
        = fmt::format_to(out_, options_.style.string, "\"{}\"", to_string(x));
      return true;
    }

    auto operator()(view<subnet> x) noexcept -> bool {
      out_
        = fmt::format_to(out_, options_.style.string, "\"{}\"", to_string(x));
      return true;
    }

    auto operator()(view<enumeration> x) noexcept -> bool {
      // We shouldn't ever arrive here as users should transform the enumeration
      // to its textual representation first, but you never really know, so
      // let's just print the number.
      out_ = fmt::format_to(out_, options_.style.number, "{}", x);
      return true;
    }

    auto operator()(const view<list>& x) noexcept -> bool {
      bool printed_once = false;
      out_ = fmt::format_to(out_, options_.style.array, "[");
      for (const auto& element : x) {
        if (should_skip(element))
          continue;
        if (!printed_once) {
          indent();
          newline();
          printed_once = true;
        } else {
          separator();
          newline();
        }
        if (!caf::visit(*this, element))
          return false;
      }
      if (printed_once) {
        dedent();
        newline();
      }
      out_ = fmt::format_to(out_, options_.style.array, "]");
      return true;
    }

    auto operator()(const view<map>& x) noexcept -> bool {
      bool printed_once = false;
      out_ = fmt::format_to(out_, options_.style.array, "[");
      for (const auto& element : x) {
        if (should_skip(element.second))
          continue;
        if (!printed_once) {
          indent();
          newline();
          printed_once = true;
        } else {
          separator();
          newline();
        }
        out_ = fmt::format_to(out_, options_.style.object, "{{");
        indent();
        newline();
        out_ = fmt::format_to(out_, options_.style.field, "\"key\": ");
        if (!caf::visit(*this, element.first))
          return false;
        separator();
        newline();
        out_ = fmt::format_to(out_, options_.style.field, "\"value\": ");
        if (!caf::visit(*this, element.second))
          return false;
        dedent();
        newline();
        out_ = fmt::format_to(out_, options_.style.object, "}}");
      }
      if (printed_once) {
        dedent();
        newline();
      }
      out_ = fmt::format_to(out_, options_.style.array, "]");
      return true;
    }

    auto operator()(const view<record>& x, std::string_view prefix
                                           = {}) noexcept -> bool {
      bool printed_once = false;
      if (!options_.flattened || prefix.empty())
        out_ = fmt::format_to(out_, options_.style.object, "{{");
      for (const auto& element : x) {
        if (should_skip(element.second))
          continue;
        if (!printed_once) {
          if (!options_.flattened) {
            indent();
            newline();
          }
          printed_once = true;
        } else {
          separator();
          newline();
        }
        if (options_.flattened) {
          const auto name = prefix.empty()
                              ? fmt::format(options_.style.field, "{}",
                                            std::string{element.first})
                              : fmt::format(options_.style.field, "{}.{}",
                                            prefix, element.first);
          if (const auto* r = caf::get_if<view<record>>(&element.second)) {
            if (!(*this)(*r, name))
              return false;
          } else {
            if (!(*this)(name))
              return false;
            out_ = fmt::format_to(out_, options_.style.object, ": ");
            if (!caf::visit(*this, element.second)) {
              return false;
            }
          }
        } else {
          out_ = fmt::format_to(out_, options_.style.field, "{}",
                                detail::json_escape(element.first));
          out_ = fmt::format_to(out_, options_.style.object, ": ");
          if (!caf::visit(*this, element.second))
            return false;
        }
      }
      if (printed_once && !options_.flattened) {
        dedent();
        newline();
      }
      if (!options_.flattened || prefix.empty())
        out_ = fmt::format_to(out_, options_.style.object, "}}");
      return true;
    }

  private:
    auto should_skip(view<data> x) noexcept -> bool {
      if (options_.omit_nulls && caf::holds_alternative<caf::none_t>(x)) {
        return true;
      }
      if (options_.omit_empty_lists && caf::holds_alternative<view<list>>(x)) {
        const auto& ys = caf::get<view<list>>(x);
        return std::all_of(ys.begin(), ys.end(),
                           [this](const view<data>& y) noexcept {
                             return should_skip(y);
                           });
      }
      if (options_.omit_empty_maps && caf::holds_alternative<view<map>>(x)) {
        const auto& ys = caf::get<view<map>>(x);
        return std::all_of(
          ys.begin(), ys.end(),
          [this](const view<map>::view_type::value_type& y) noexcept {
            return should_skip(y.second);
          });
      }
      if (options_.omit_empty_records
          && caf::holds_alternative<view<record>>(x)) {
        const auto& ys = caf::get<view<record>>(x);
        return std::all_of(
          ys.begin(), ys.end(),
          [this](const view<record>::view_type::value_type& y) noexcept {
            return should_skip(y.second);
          });
      }
      return false;
    }

    void indent() noexcept {
      indentation_ += options_.indentation;
    }

    void dedent() noexcept {
      TENZIR_ASSERT_EXPENSIVE(indentation_ >= options_.indentation,
                              "imbalanced calls between indent() and dedent()");
      indentation_ -= options_.indentation;
    }

    void separator() noexcept {
      if (options_.oneline)
        out_ = fmt::format_to(out_, options_.style.comma, ", ");
      else
        out_ = fmt::format_to(out_, options_.style.comma, ",");
    }

    void newline() noexcept {
      if (!options_.oneline)
        out_ = fmt::format_to(out_, "\n{: >{}}", "", indentation_);
    }

    Iterator& out_;
    const json_printer_options& options_;
    uint32_t indentation_ = 0;
  };

  template <class Iterator>
  auto print(Iterator& out, const view<data>& d) const noexcept -> bool {
    return caf::visit(print_visitor{out, options_}, d);
  }

  template <class Iterator, class T>
    requires caf::detail::tl_contains<view<data>::types, T>::value
  auto print(Iterator& out, const T& d) const noexcept -> bool {
    return print_visitor{out, options_}(d);
  }

  template <class Iterator>
  auto print(Iterator& out, const data& d) const noexcept -> bool {
    return print(out, make_view(d));
  }

  template <class Iterator, class T>
    requires(!caf::detail::tl_contains<view<data>::types, T>::value
             && caf::detail::tl_contains<data::types, T>::value)
  auto print(Iterator& out, const T& d) const noexcept -> bool {
    return print(out, make_view(d));
  }

private:
  json_printer_options options_ = {};
};

} // namespace tenzir
