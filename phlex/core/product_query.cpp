#include "phlex/core/product_query.hpp"

#include "fmt/format.h"

namespace phlex {
  // Check that all products selected by /other/ would satisfy this query
  bool product_query::match(product_query const& other) const
  {
    using experimental::identifier;
    if (identifier(creator) != identifier(other.creator)) {
      return false;
    }
    if (identifier(layer) != identifier(other.layer)) {
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
    // string comparisons for now for a gradual transition
    if (creator.get_string_view() != spec.algorithm()) {
      return false;
    }
    if (type != spec.type()) {
      return false;
    }
    if (suffix) {
      if (std::string_view(*suffix) != spec.name()) {
        return false;
      }
    }
    return true;
  }

  std::string product_query::to_string() const
  {
    if (suffix) {
      return fmt::format("{}/{} ϵ {}", creator, *suffix, layer);
    }
    return fmt::format("{} ϵ {}", creator, layer);
  }

  experimental::product_specification product_query::spec() const
  {
    if (!suffix) {
      throw std::logic_error("Product suffixes are (temporarily) mandatory");
    }
    // Not efficient, but this should be temporary
    return experimental::product_specification::create(std::string(*suffix));
  }
}
