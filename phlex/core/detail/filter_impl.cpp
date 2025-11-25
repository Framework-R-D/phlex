#include "phlex/core/detail/filter_impl.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/fwd.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace {
  phlex::experimental::specified_label const output_dummy{.name=phlex::experimental::qualified_name{
    phlex::experimental::algorithm_name{"for_output_only", ""}, "for_output_only"}};
  std::vector<phlex::experimental::specified_label> const for_output_only{output_dummy};
}

namespace phlex::experimental {
  decision_map::decision_map(unsigned int total_decisions) : total_decisions_{total_decisions} {}

  void decision_map::update(predicate_result result)
  {
    decltype(results_)::accessor a;
    results_.insert(a, result.msg_id);

    // First check that the decision is not already complete
    if (is_complete(a->second)) {
      return;
    }

    if (not result.result) {
      a->second = false_value;
      return;
    }

    ++a->second;

    if (a->second == total_decisions_) {
      a->second = true_value;
    }
  }

  auto decision_map::value(std::size_t const msg_id) const -> unsigned int
  {
    decltype(results_)::const_accessor a;
    if (results_.find(a, msg_id)) {
      return a->second;
    }
    return 0u;
  }

  auto decision_map::claim(accessor& a, std::size_t const msg_id) -> bool
  {
    return results_.find(a, msg_id);
  }

  void decision_map::erase(accessor& a) { results_.erase(a); }

  data_map::data_map(specified_labels const& product_names) :
    product_names_{&product_names}, nargs_{product_names.size()}
  {
    assert(nargs_ > 0);
  }

  data_map::data_map(for_output_t) : data_map{for_output_only} {}

  void data_map::update(std::size_t const msg_id, product_store_const_ptr const& store)
  {
    decltype(stores_)::accessor a;
    if (stores_.insert(a, msg_id)) {
      a->second = std::vector<product_store_const_ptr>(nargs_);
    }
    auto& elem = a->second;
    if (nargs_ == 1ull) {
      // We do not check that the product is in the store if only one argument is
      // forwarded.  This enables us to forward arguments for regular nodes and also
      // output nodes, which do not take individual data products.
      elem[0] = store;
      return;
    }

    // Fill slots in the order of the input arguments to the downstream node.
    for (std::size_t i = 0; i != nargs_; ++i) {
      if (elem[i] or not store->contains_product((*product_names_)[i].name.full()))
        continue;
      elem[i] = store;
    }
  }

  auto data_map::is_complete(std::size_t const msg_id) const -> bool
  {
    decltype(stores_)::const_accessor a;
    if (stores_.find(a, msg_id)) {
      return a->second.size() == nargs_;
    }
    return false;
  }

  auto data_map::release_data(std::size_t const msg_id) -> std::vector<product_store_const_ptr>
  {
    std::vector<product_store_const_ptr> result;
    if (decltype(stores_)::accessor a; stores_.find(a, msg_id)) {
      result = std::move(a->second);
      stores_.erase(a);
    }
    return result;
  }
}
