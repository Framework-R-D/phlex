#include "phlex/model/detail/excerpt.hpp"

#include <fmt/format.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace phlex::detail {
  std::string excerpt(std::string_view text)
  {
    std::string shown{text.substr(0, max_excerpt_length)};
    if (text.size() > max_excerpt_length) {
      shown += "...";
    }
    return fmt::format("{:?}", shown);
  }
}
