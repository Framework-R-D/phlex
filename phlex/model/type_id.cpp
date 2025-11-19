#include "phlex/model/type_id.hpp"

#include "fmt/ranges.h"
#include <string>

auto fmt::formatter<phlex::experimental::type_id>::format(phlex::experimental::type_id type,
                                                          format_context& ctx) const
{
  using namespace std::string_literals;
  using namespace phlex::experimental;
  if (!type.valid()) {
    return fmt::formatter<std::string>::format("INVALID / EMPTY"s, ctx);
  }
  if (type.has_children()) {
    std::string const out = fmt::format("STRUCT {{{}}}", fmt::join(type.children_, ", "));
    return fmt::formatter<std::string>::format(out, ctx);
  }

  std::string fundamental = "void"s;
  switch (type.fundamental()) {
  case type_id::builtin::void_v:
    fundamental = "void"s;
    break;
  case type_id::builtin::bool_v:
    fundamental = "bool"s;
    break;
  case type_id::builtin::char_v:
    fundamental = "char"s;
    break;
  case type_id::builtin::int_v:
    fundamental = "int"s;
    break;

  case type_id::builtin::short_v:
    fundamental = "short"s;
    break;

  case type_id::builtin::long_v:
    fundamental = "long"s;
    break;

  case type_id::builtin::long_long_v:
    fundamental = "long long"s;
    break;

  case type_id::builtin::float_v:
    fundamental = "float"s;
    break;

  case type_id::builtin::double_v:
    fundamental = "double"s;
    break;
  }
  std::string const out = fmt::format("{}{}{}",
                                      type.is_list() ? "LIST "s : ""s,
                                      type.is_unsigned() ? "unsigned "s : ""s,
                                      fundamental);
  return fmt::formatter<std::string>::format(out, ctx);
}
