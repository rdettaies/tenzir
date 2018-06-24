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

#include "vast/command.hpp"

#include "vast/logger.hpp"

namespace vast {

command::command() : parent_(nullptr) {
  // nop
}

command::command(command* parent, std::string_view name)
  : parent_{parent},
    name_{name} {
  // nop
}

command::~command() {
  // nop
}

int command::run(caf::actor_system& sys, caf::config_value_map& options,
                     argument_iterator begin, argument_iterator end) {
  VAST_TRACE(VAST_ARG(std::string(name_)), VAST_ARG("args", begin, end),
             VAST_ARG(options));
  using caf::get_or;
  // Parse arguments for this command.
  auto [state, position] = opts_.parse(options, begin, end);
  bool has_subcommand;
  switch(state) {
    default:
      std::cerr << parse_error(state, position, begin, end) << std::endl;
      return EXIT_FAILURE;
    case caf::pec::success:
      has_subcommand = false;
      break;
    case caf::pec::not_an_option:
      has_subcommand = position != end;
      break;
  }
  // Check for help option.
  if (get_or<boolean>(options, "help", false)) {
    std::cerr << usage() << std::endl;
    return EXIT_SUCCESS;
  }
  // Check whether the options allow for further processing.
  switch (proceed(sys, options, position, end)) {
    default:
        // nop
        break;
    case stop_successful:
      return EXIT_SUCCESS;
    case stop_with_error:
      return EXIT_FAILURE;
  }
  // Invoke run_impl if no subcommand was defined.
  if (!has_subcommand)
    return run_impl(sys, options, position, end);
  // Consume CLI arguments if we have arguments but don't have subcommands.
  if (nested_.empty())
    return run_impl(sys, options, position, end);
  // Dispatch to subcommand.
  auto i = nested_.find(*position);
  if (i == nested_.end()) {
    std::cerr << unknown_subcommand_error(position, end);
    return EXIT_FAILURE;
  }
  return i->second->run(sys, options, position + 1, end);
}

int command::run(caf::actor_system& sys, argument_iterator begin,
                 argument_iterator end) {
  caf::config_value_map options;
  return run(sys, options, begin, end);
}

std::string command::usage() const {
  std::stringstream result;
  result << opts_.help_text() << "\n";
  result << "\nSubcommands:\n";
  for (auto& kvp : nested_)
    result << "  " << kvp.first << "\n";
  return result.str();
}

std::string command::full_name() const {
  std::string result;
  if (is_root())
    return result;
  result = parent_->full_name();
  if (!result.empty())
    result += ' ';
  result += name_;
  return result;
}

command::proceed_result command::proceed(caf::actor_system&,
                                         const caf::config_value_map& options,
                                         argument_iterator begin,
                                         argument_iterator end) {
  VAST_UNUSED(options, begin, end);
  VAST_TRACE(VAST_ARG(std::string{name_}), VAST_ARG("args", begin, end),
             VAST_ARG(options));
  return proceed_ok;
}

int command::run_impl(caf::actor_system&, const caf::config_value_map& options,
                      argument_iterator begin, argument_iterator end) {
  VAST_UNUSED(options, begin, end);
  VAST_TRACE(VAST_ARG(std::string{name_}), VAST_ARG("args", begin, end),
             VAST_ARG(options));
  usage();
  return EXIT_FAILURE;
}

std::string command::parse_error(caf::pec code,
                                 argument_iterator error_position,
                                 argument_iterator begin,
                                 argument_iterator end) const {
  std::stringstream result;
  result << "Failed to parse command" << "\n";
  result << "  Command: " << name() << " " << detail::join(begin, end, " ")
         << "\n";
  result << "  Description: " << to_string(code) << "\n";
  if (error_position != end)
    result << "  Position: " << *error_position << "\n";
  else
    result << "  Position: at the end\n";
  result << "\n" << usage();
  return result.str();
}

std::string command::unknown_subcommand_error(argument_iterator error_position,
                                              argument_iterator end) const {
  VAST_ASSERT(error_position != end);
  std::stringstream result;
  result << "No such command: " << full_name() << " " << *error_position
         << "\n\n"
         << usage();
  return result.str();
}

} // namespace vast
