#ifndef TEST_PRODUCT_STORE_HELPERS_HPP
#define TEST_PRODUCT_STORE_HELPERS_HPP

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/type_id.hpp"

#include <gsl/pointers>

#include <utility>

namespace phlex::test {
  template <typename T>
  experimental::product_specification spec(experimental::algorithm_name const& creator)
  {
    return {creator, experimental::identifier{""}, experimental::make_type_id<T>()};
  }

  template <typename T>
  experimental::product_store_ptr store_with_product(
    gsl::not_null<experimental::algorithm_name const*> creator,
    gsl::not_null<experimental::identifier const*> stage,
    T value)
  {
    auto store = experimental::product_store::base(creator, stage);
    store->add_product(spec<T>(*creator), std::move(value));
    return store;
  }
}

#endif // TEST_PRODUCT_STORE_HELPERS_HPP
