#include "phlex/core/product_query.hpp"

#include "fmt/format.h"

namespace phlex {
  void product_query::set_type(experimental::type_id&& type) { type_id_ = std::move(type); }

  // Check that all products selected by /other/ would satisfy this query
  bool product_query::match(product_query const& other) const
  {
    if (creator_ != other.creator_) {
      return false;
    }
    if (layer_ != other.layer_) {
      return false;
    }
    if (suffix_ && suffix_ != other.suffix_) {
      return false;
    }
    if (stage_ && stage_ != other.stage_) {
      return false;
    }
    // Special case. If other has an unset type_id, ignore this in case it just hasn't been set yet
    if (type_id_.valid() && other.type_id_.valid() && type_id_ != other.type_id_) {
      return false;
    }
    return true;
  }

  // Check if a product_specification satisfies this query
  bool product_query::match(experimental::product_specification const& spec) const
  {
    if (creator_ != spec.algorithm()) {
      return false;
    }
    if (type_id_ != spec.type()) {
      return false;
    }
    if (suffix_) {
      if (*suffix_ != spec.name()) {
        return false;
      }
    }
    return true;
  }

  std::string product_query::to_string() const
  {
    if (suffix_) {
      return fmt::format("{}/{} ϵ {}", creator_, *suffix_, layer_);
    }
    return fmt::format("{} ϵ {}", creator_, layer_);
  }

  experimental::product_specification product_query::spec() const
  {
    if (!suffix()) {
      throw std::logic_error("Product suffixes are (temporarily) mandatory");
    }
    return experimental::product_specification::create(*suffix());
  }
}
