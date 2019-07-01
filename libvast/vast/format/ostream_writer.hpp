/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#pragma once

#include <iosfwd>
#include <memory>
#include <string_view>
#include <vector>

#include <caf/error.hpp>

#include "vast/error.hpp"
#include "vast/format/writer.hpp"
#include "vast/table_slice.hpp"

namespace vast::format {

class ostream_writer : public writer {
public:
  // -- member types -----------------------------------------------------------

  using ostream_ptr = std::unique_ptr<std::ostream>;

  // -- constructors, destructors, and assignment operators --------------------

  ostream_writer(ostream_ptr out);

  ostream_writer() = default;

  ostream_writer(ostream_writer&&) = default;

  ostream_writer& operator=(ostream_writer&&) = default;

  ~ostream_writer() override;

  // -- overrides --------------------------------------------------------------

  caf::expected<void> flush() override;

protected:
  /// Appends `x` to `buf_`.
  void append(std::string_view x) {
    buf_.insert(buf_.end(), x.begin(), x.end());
  }

  /// Appends `x` to `buf_`.
  void append(char x) {
    buf_.emplace_back(x);
  }

  template <bool IncludeFieldNames, class Printer>
  caf::error print(Printer& printer, const table_slice& x,
                   std::string_view begin_of_line, std::string_view separator,
                   std::string_view end_of_line) {
    auto at = [&](size_t row, size_t column) {
      if constexpr (IncludeFieldNames)
        return std::pair{x.column_name(column), x.at(row, column)};
      else
        return x.at(row, column);
    };
    auto iter = std::back_inserter(buf_);
    for (size_t row = 0; row < x.rows(); ++row) {
      append(begin_of_line);
      if (!printer.print(iter, at(row, 0)))
        return ec::print_error;
      for (size_t column = 1; column < x.columns(); ++column) {
        append(separator);
        if (!printer.print(iter, at(row, column)))
          return ec::print_error;
      }
      append(end_of_line);
      append('\n');
      write_buf();
    }
    return caf::none;
  }

  /// Writes the content of `buf_` to `out_` and clears `buf_` afterwards.
  void write_buf();

  // Buffer for building lines before syncing writing to out_.
  std::vector<char> buf_;

  // Output stream for writing to STDOUT or disk.
  ostream_ptr out_;
};

} // namespace vast::format
