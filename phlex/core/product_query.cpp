#include "phlex/core/product_query.hpp"

#include "fmt/format.h"

namespace phlex {
  // Check that all products selected by /other/ would satisfy this query
  bool product_query::match(product_query const& other) const
  {
    using experimental::identifier;
    if (creator && creator != other.creator) {
      return false;
    }
    if (layer && layer != other.layer) {
      return false;
    }
    if (suffix && suffix != other.suffix) {
      return false;
    }
    if (stage && stage != other.stage) {
      return false;
    }
    // Special case. If other has an unset type_id, ignore this in case it just hasn't been set yet
    if (type.valid() && other.type.valid() && type != other.type) {
      return false;
    }
    return true;
  }

  // Check if a product_specification satisfies this query
  bool product_query::match(experimental::product_specification const& spec) const
  {
    if (creator && creator != spec.algorithm() && creator != spec.plugin()) {
      return false;
    }
    if (type != spec.type()) {
      return false;
    }
    if (suffix && suffix != spec.suffix()) {
      return false;
    }
    return true;
  }

  std::string product_query::to_string() const
  {
    std::string_view creator_sv = creator ? std::string_view(*creator) : "[ANY]";
    std::string_view layer_sv = layer ? std::string_view(*layer) : "[ANY]";
    std::string_view suffix_sv = suffix ? std::string_view(*suffix) : "[ANY]";
    return fmt::format("{}/{} ϵ {}", creator_sv, suffix_sv, layer_sv);
  }

  bool product_query::operator==(product_query const& rhs) const
  {
    using experimental::identifier;
    return (type == rhs.type) && (creator == rhs.creator) && (layer == rhs.layer) &&
           (suffix == rhs.suffix) && (stage == rhs.stage);
  }
  std::strong_ordering product_query::operator<=>(product_query const& rhs) const
  {
    using experimental::identifier;
    return std::tie(type, creator, layer, suffix, stage) <=>
           std::tie(rhs.type, rhs.creator, rhs.layer, rhs.suffix, rhs.stage);
  }

  experimental::product_specification const* resolve_in_store(
    product_query const& query, experimental::product_store const& store)
  {
    for (auto const& [spec, _] : store) {
      if (query.match(spec)) {
        return &spec;
      }
    }
    return nullptr;
  }
}
