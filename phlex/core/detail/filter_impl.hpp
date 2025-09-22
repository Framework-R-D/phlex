#ifndef phlex_core_detail_filter_impl_hpp
#define phlex_core_detail_filter_impl_hpp

#include "phlex/core/fwd.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/product_store.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"

#include <cassert>

namespace phlex::experimental {
  struct predicate_result {
    end_of_message_ptr eom;
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

  class decision_map {
  public:
    explicit decision_map(unsigned int total_decisions);

    void update(predicate_result result);
    unsigned int value(std::size_t msg_id) const;
    void erase(std::size_t msg_id);

  private:
    unsigned int const total_decisions_;
    oneapi::tbb::concurrent_hash_map<std::size_t, unsigned int> results_;
  };

  class data_map {
    using stores_t =
      oneapi::tbb::concurrent_hash_map<std::size_t, std::vector<product_store_const_ptr>>;

  public:
    using accessor = stores_t::accessor;

    struct for_output_t {};
    static constexpr for_output_t for_output{};
    explicit data_map(for_output_t);
    explicit data_map(specified_labels const& product_names);

    bool is_complete(std::size_t const msg_id) const;

    void update(std::size_t const msg_id, product_store_const_ptr const& store);
    std::vector<product_store_const_ptr> release_data(accessor& a, std::size_t const msg_id);

  private:
    stores_t stores_;
    std::vector<specified_label> const* product_names_;
    std::size_t nargs_;
  };
}

#endif // phlex_core_detail_filter_impl_hpp
