#include "phlex/core/dot/attributes.hpp"
#include <string>

namespace {
  auto maybe_comma(std::string const& result) -> std::string { return result.empty() ? "" : ", "; }
}

namespace phlex::experimental::dot {
  auto to_string(attributes const& attrs) -> std::string
  {
    std::string result{};
    if (not attrs.color.empty()) {
      result += "color=" + attrs.color;
    }
    if (not attrs.fontcolor.empty()) {
      result += maybe_comma(result) + "fontcolor=" + attrs.fontcolor;
    }
    if (not attrs.fontsize.empty()) {
      result += maybe_comma(result) + "fontsize=" + attrs.fontsize;
    }
    if (not attrs.label.empty()) {
      result += maybe_comma(result) + "label=\" " + attrs.label + "\"";
    }
    if (not attrs.shape.empty()) {
      result += maybe_comma(result) + "shape=" + attrs.shape;
    }
    if (not attrs.style.empty()) {
      result += maybe_comma(result) + "style=" + attrs.style;
    }
    return "[" + result + "]";
  }

  auto parenthesized(std::string const& n) -> std::string { return "(" + n + ")"; }
}
