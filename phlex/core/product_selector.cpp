#include "phlex/core/product_selector.hpp"

#include "fmt/format.h"

namespace phlex {
  // Check that all products selected by /other/ would satisfy this query
  bool product_selector::match(product_selector const& other) const
  {
    if (creator && creator != other.creator) {
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
  bool product_selector::match(detail::product_specification const& spec) const
  {
    if (!creator_match(spec.creator())) {
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
  bool product_selector::match(detail::product_specification const& spec,
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
    if (!creator) {
      return true; // empty creator matches everything
    }
    experimental::identifier const& creator{*(this->creator)};
    return alg.plugin() == creator || alg.algorithm() == creator;
  }

  std::string product_selector::to_string() const
  {
    // Will generate <<suffix>::<concept> by <creator> (of <stage>) in <layer>>
    using experimental::identifier;
    std::string_view suffix_str =
      suffix.transform(&identifier::operator std::string_view).value_or("[ANY]");
    std::string type_str =
      this->type.valid() ? fmt::format("[{}]", this->type) : "[UNSET]"; // will later be concept
    auto layer_str = std::string_view(layer);
    std::string_view creator_str = creator ? std::string_view(*creator) : "[ANY]";
    std::string_view stage_str =
      stage.transform(&identifier::operator std::string_view).value_or("[ANY STAGE]");

    return fmt::format(
      "<{}::{} by {} (of {}) in {}>", suffix_str, type_str, creator_str, stage_str, layer_str);
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
    return std::tie(type, creator, static_cast<identifier const&>(layer), suffix, stage) <=>
           std::tie(rhs.type,
                    rhs.creator,
                    static_cast<identifier const&>(rhs.layer),
                    rhs.suffix,
                    rhs.stage);
  }

  detail::product_specification const* resolve_in_store(product_selector const& query,
                                                        experimental::product_store const& store)
  {
    for (auto const& [spec, _] : store) {
      if (query.match(spec)) {
        return &spec;
      }
    }
    return nullptr;
  }
}
