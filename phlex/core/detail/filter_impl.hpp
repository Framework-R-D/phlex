#ifndef PHLEX_CORE_DETAIL_FILTER_IMPL_HPP
#define PHLEX_CORE_DETAIL_FILTER_IMPL_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex_core_export.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"

#include <cassert>

namespace phlex::experimental {
  struct predicate_result {
    std::size_t msg_id;
    bool result;
  };

  inline constexpr unsigned int true_value{-1u};
  inline constexpr unsigned int false_value{-2u};

  constexpr bool is_complete(unsigned int value)
  {
    return value == true_value or value == false_value;
  }

  constexpr bool to_boolean(unsigned int value)
  {
    assert(is_complete(value));
    return value == true_value;
  }

  class phlex_core_EXPORT decision_map {
    using decisions_t = oneapi::tbb::concurrent_hash_map<std::size_t, unsigned int>;

  public:
    using accessor = decisions_t::accessor;

    explicit decision_map(unsigned int total_decisions);

    void update(predicate_result result);
    unsigned int value(std::size_t msg_id) const;
    void erase(accessor& a);
    bool claim(accessor& a, std::size_t msg_id);

  private:
    unsigned int const total_decisions_;
    decisions_t results_;
  };

  class phlex_core_EXPORT data_map {
    using stores_t =
      oneapi::tbb::concurrent_hash_map<std::size_t, std::vector<product_store_const_ptr>>;

  public:
    struct for_output_t {};
    static constexpr for_output_t for_output{};
    explicit data_map(for_output_t);
    explicit data_map(product_queries const& product_names);

    bool is_complete(std::size_t const msg_id) const;

    void update(std::size_t const msg_id, product_store_const_ptr const& store);
    std::vector<product_store_const_ptr> release_data(std::size_t const msg_id);

  private:
    stores_t stores_;
    std::vector<product_query> const* product_names_;
    std::size_t nargs_;
  };
}

#endif // PHLEX_CORE_DETAIL_FILTER_IMPL_HPP
