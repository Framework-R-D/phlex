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
    experimental::identifier tmp_creator{this->creator};
    if (tmp_creator != spec.algorithm() && tmp_creator != spec.plugin()) {
      return false;
    }
    if (type != spec.type()) {
      return false;
    }
    if (suffix) {
      if (*suffix != spec.name()) {
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
    using namespace phlex::experimental;
    auto const& creator_identifier = static_cast<experimental::identifier const&>(creator);
    return product_specification{
      algorithm_name::create(static_cast<std::string_view>(creator_identifier)), *suffix, type};
  }

  bool product_query::operator==(product_query const& rhs) const
  {
    using experimental::identifier;
    return (type == rhs.type) && (identifier(creator) == identifier(rhs.creator)) &&
           (identifier(layer) == identifier(rhs.layer)) && (suffix == rhs.suffix) &&
           (stage == rhs.stage);
  }
  std::strong_ordering product_query::operator<=>(product_query const& rhs) const
  {
    using experimental::identifier;
    return std::tie(type,
                    static_cast<identifier const&>(creator),
                    static_cast<identifier const&>(layer),
                    suffix,
                    stage) <=> std::tie(rhs.type,
                                        static_cast<identifier const&>(rhs.creator),
                                        static_cast<identifier const&>(rhs.layer),
                                        rhs.suffix,
                                        rhs.stage);
  }
}
