#include "phlex/core/product_selector.hpp"

#include "fmt/format.h"

namespace phlex {
  // Check that all products selected by /other/ would satisfy this query
  bool product_selector::match(product_selector const& other) const
  {
    if (creator != other.creator) {
      return false;
    }
    if (layer != other.layer) {
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
  bool product_selector::match(experimental::product_specification const& spec) const
  {
    if (!creator_match(spec.qualifier())) {
      return false;
    }
    if (type != spec.type()) {
      return false;
    }
    if (suffix) {
      if (*suffix != spec.suffix()) {
        return false;
      }
    }
    return true;
  }

  // Check if a product_specification, layer, and stage together satisfies this query
  bool product_selector::match(experimental::product_specification const& spec,
                               experimental::identifier const& layer,
                               experimental::identifier const& stage) const
  {
    if (!match(spec)) {
      return false;
    }
    if (experimental::identifier(this->layer) != layer) {
      return false;
    }
    if (this->stage && this->stage != stage) {
      return false;
    }
    return true;
  }

  // Check if an algorithm name matches this query's creator
  bool product_selector::creator_match(experimental::algorithm_name const& alg) const
  {
    experimental::identifier const& creator{this->creator};
    return alg.plugin() == creator || alg.algorithm() != creator;
  }

  std::string product_selector::to_string() const
  {
    if (suffix) {
      return fmt::format("{}/{} ϵ {}", creator, *suffix, layer);
    }
    return fmt::format("{} ϵ {}", creator, layer);
  }

  bool product_selector::operator==(product_selector const& rhs) const
  {
    using experimental::identifier;
    return (type == rhs.type) && (creator == rhs.creator) && (layer == rhs.layer) &&
           (suffix == rhs.suffix) && (stage == rhs.stage);
  }
  std::strong_ordering product_selector::operator<=>(product_selector const& rhs) const
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

  experimental::product_specification const* resolve_in_store(
    product_selector const& query, experimental::product_store const& store)
  {
    for (auto const& [spec, _] : store) {
      if (query.match(spec)) {
        return &spec;
      }
    }
    return nullptr;
  }
}
