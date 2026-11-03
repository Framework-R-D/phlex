#include "phlex/model/type_id.hpp"

#include <boost/container_hash/hash.hpp>
#include <boost/core/demangle.hpp>
#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace phlex::experimental {
  std::string type_id::exact_name() const { return boost::core::demangle(exact_->name()); }

  std::size_t hash_value(type_id const& id)
  {
    std::size_t hash = std::hash<unsigned char>{}(id.id_);
    if (id.has_children()) {
      boost::hash_combine(hash, id.children_);
    }
    return hash;
  }
}

fmt::format_context::iterator fmt::formatter<phlex::experimental::type_id>::format(
  phlex::experimental::type_id type, format_context& ctx) const
{
  using namespace std::string_literals;
  using namespace phlex::experimental;
  if (!type.valid()) {
    return fmt::formatter<std::string>::format("INVALID / EMPTY"s, ctx);
  }
  if (type.has_children()) {
    std::string const out = fmt::format(
      "{}STRUCT {{{}}}", type.is_list() ? "LIST " : "", fmt::join(type.children_, ", "));
    return fmt::formatter<std::string>::format(out, ctx);
  }

  // Valid non-aggregate IDs use the contiguous builtin values, starting with void_v = 0.
  static constexpr std::array<std::string_view, 10> fundamental_names{
    "void", "bool", "char", "int", "short", "long", "long long", "float", "double", "long double"};
  auto const fundamental = fundamental_names[static_cast<std::size_t>(type.fundamental())];
  std::string const out = fmt::format("{}{}{}",
                                      type.is_list() ? "LIST "s : ""s,
                                      type.is_unsigned() ? "unsigned "s : ""s,
                                      fundamental);
  return fmt::formatter<std::string>::format(out, ctx);
}
