#ifndef PHLEX_MODEL_FULL_PRODUCT_SPEC_HPP
#define PHLEX_MODEL_FULL_PRODUCT_SPEC_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include "boost/container_hash/hash.hpp"
#include "fmt/format.h"

namespace phlex::experimental {
  class PHLEX_MODEL_EXPORT full_product_spec {
  public:
    full_product_spec(product_specification spec, identifier layer, identifier stage) :
      spec_{std::move(spec)}, layer_{std::move(layer)}, stage_{std::move(stage)}
    {
    }

    bool operator==(full_product_spec const&) const noexcept = default;

    product_specification const& spec() const noexcept { return spec_; }
    algorithm_name const& creator() const noexcept { return spec_.qualifier(); }
    identifier const& suffix() const noexcept { return spec_.suffix(); }
    type_id type() const noexcept { return spec_.type(); }
    void set_type(type_id&& type) { spec_.set_type(std::move(type)); }
    identifier const& layer() const noexcept { return layer_; }
    identifier const& stage() const noexcept { return stage_; }
    std::size_t hash() const noexcept
    {
      std::size_t result = creator().plugin().hash();
      boost::hash_combine(result, creator().algorithm().hash());
      boost::hash_combine(result, suffix().hash());
      boost::hash_combine(result, layer().hash());
      boost::hash_combine(result, stage().hash());
      boost::hash_combine(result, type());
      return result;
    }

    std::string to_string() const
    {
      return fmt::format(
        "{}:{}/{} ϵ {}", creator().plugin(), creator().algorithm(), suffix(), layer());
    }

  private:
    product_specification spec_;
    identifier layer_;
    identifier stage_;
  };
}

#endif // PHLEX_MODEL_FULL_PRODUCT_SPEC_HPP
