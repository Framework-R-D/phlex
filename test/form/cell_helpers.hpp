#ifndef TEST_FORM_CELL_HELPERS_HPP
#define TEST_FORM_CELL_HELPERS_HPP

#include "core/cell_index.hpp"

#include <cstdint>
#include <string>

namespace form::test {
  /// Single-layer data cell.
  inline ::form::detail::experimental::cell_index event_cell(std::uint64_t event)
  {
    return ::form::detail::experimental::cell_index{.id = "[event:" + std::to_string(event) + "]",
                                                    .hierarchy = {{"event"}},
                                                    .layer_values = {event}};
  }

  /// Two-layer data cell.
  inline ::form::detail::experimental::cell_index event_segment_cell(std::uint64_t event,
                                                                     std::uint64_t segment)
  {
    return ::form::detail::experimental::cell_index{
      .id = "[event:" + std::to_string(event) + ", segment:" + std::to_string(segment) + "]",
      .hierarchy = {{"event", "segment"}},
      .layer_values = {event, segment}};
  }
}

#endif // TEST_FORM_CELL_HELPERS_HPP
