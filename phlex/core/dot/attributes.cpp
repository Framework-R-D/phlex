#include "phlex/core/dot/attributes.hpp"

#include <string>

namespace {
  std::string maybe_comma(std::string const& result) { return result.empty() ? "" : ", "; }
}

namespace phlex::experimental::dot {
  std::string to_string(attributes const& attrs)
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

  std::string parenthesized(std::string const& n) { return "(" + n + ")"; }
}
