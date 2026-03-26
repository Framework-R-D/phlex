#include "phlex/core/product_registry.hpp"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <ranges>

namespace phlex::experimental {
  namespace {
    template <typename T>
    using set_t = std::set<T>;
    std::string store_listing_for_error(product_registry::store_t const& store)
    {
      namespace views = std::ranges::views;
      auto list = store | views::transform([](detail::full_product_spec const& spec) {
                    return fmt::format("    - {}:{}/{} [{}] ∈ {} FROM {}",
                                       spec.plugin(),
                                       spec.algorithm(),
                                       spec.suffix(),
                                       spec.type(),
                                       spec.layer,
                                       spec.stage);
                  });
      return fmt::format("{}", fmt::join(list, "\n"));
    }

    // convert the pair of iterators returned from equal_range to a set
    template <typename IterToPair>
    auto make_set(std::pair<IterToPair, IterToPair> const& its)
    {
      using ret_t = set_t<typename IterToPair::value_type::second_type>;
      auto [b, e] = its;
      return std::ranges::subrange(b, e) | std::ranges::views::values | std::ranges::to<ret_t>();
    }

    template <typename T>
    set_t<T> intersect(set_t<T> const& lhs, set_t<T> const& rhs)
    {
      set_t<T> result;
      std::ranges::set_intersection(lhs, rhs, std::inserter(result, result.begin()));
      return result;
    }
  }
  // Add the product to store_ and add entries to each map
  void product_registry::add_product(product_specification spec, identifier layer, identifier stage)
  {
    auto* ptr = &store_.emplace_front(std::move(spec), std::move(layer), std::move(stage));
    spdlog::debug("Adding product {}:{}/{} [{}] ∈ {}",
                  ptr->plugin(),
                  ptr->algorithm(),
                  ptr->suffix(),
                  ptr->type(),
                  ptr->layer);
    plugin_map_.emplace(std::cref(ptr->plugin()), ptr);
    algorithm_map_.emplace(std::cref(ptr->algorithm()), ptr);
    suffix_map_.emplace(std::cref(ptr->suffix()), ptr);
    layer_map_.emplace(std::cref(ptr->layer), ptr);
    type_map_.emplace(ptr->type(), ptr);
  }

  // Lookup a product query by checking each map and finding the intersection of the results
  detail::full_product_spec const& product_registry::lookup(product_query const& query) const
  {
    std::forward_list<result_set_t> result_sets;
    if (query.type.valid()) {
      auto res = make_set(type_map_.equal_range(query.type));
      if (res.empty()) {
        throw std::runtime_error(fmt::format(
          "Could not find {} in product registry (no matching type). Contents are:\n{}\n",
          query,
          store_listing_for_error(store_)));
      }
      result_sets.push_front(std::move(res));
    }
    if (query.creator) {
      auto creator = std::string_view(*query.creator);
      // If the creator name contains a `:` we have both a plugin name and an algorithm name.
      // Otherwise the creator name can match the plugin OR the algorithm
      if (creator.contains(':')) {
        auto plugin = identifier(creator.substr(0, creator.find(':')));
        auto algorithm = identifier(creator.substr(creator.find(':') + 1));
        auto plugin_res = make_set(plugin_map_.equal_range(plugin));
        if (plugin_res.empty()) {
          throw std::runtime_error(fmt::format("Could not find {} in product registry (no matching "
                                               "creator plugin). Contents are:\n{}\n",
                                               query,
                                               store_listing_for_error(store_)));
        }
        result_sets.push_front(std::move(plugin_res));
        auto algo_res = make_set(algorithm_map_.equal_range(algorithm));
        if (algo_res.empty()) {
          throw std::runtime_error(fmt::format("Could not find {} in product registry (no matching "
                                               "creator algorithm). Contents are:\n{}\n",
                                               query,
                                               store_listing_for_error(store_)));
        }
        result_sets.push_front(std::move(algo_res));
      } else {
        auto plugin_res = make_set(plugin_map_.equal_range(*query.creator));
        auto algo_res = make_set(algorithm_map_.equal_range(*query.creator));
        plugin_res.merge(std::move(algo_res));
        if (plugin_res.empty()) {
          throw std::runtime_error(fmt::format(
            "Could not find {} in product registry (no matching creator). Contents are:\n{}\n",
            query,
            store_listing_for_error(store_)));
        }
        result_sets.push_front(std::move(plugin_res));
      }
    }
    if (query.suffix) {
      auto res = make_set(suffix_map_.equal_range(*query.suffix));
      if (res.empty()) {
        throw std::runtime_error(fmt::format(
          "Could not find {} in product registry (no matching suffix). Contents are:\n{}\n",
          query,
          store_listing_for_error(store_)));
      }
      result_sets.push_front(std::move(res));
    }
    if (query.layer) {
      auto res = make_set(layer_map_.equal_range(*query.layer));
      if (res.empty()) {
        throw std::runtime_error(fmt::format(
          "Could not find {} in product registry (no matching layer). Contents are:\n{}\n",
          query,
          store_listing_for_error(store_)));
      }
      result_sets.push_front(std::move(res));
    }
    if (query.stage) {
      auto res = make_set(stage_map_.equal_range(*query.stage));
      if (res.empty()) {
        throw std::runtime_error(fmt::format(
          "Could not find {} in product registry (no matching stage). Contents are:\n{}\n",
          query,
          store_listing_for_error(store_)));
      }
      result_sets.push_front(std::move(res));
    }

    if (result_sets.empty()) {
      throw std::runtime_error(fmt::format("Query {} is empty!", query));
    }
    auto result = *std::ranges::fold_left_first(result_sets, intersect<result_set_t::key_type>);
    if (result.empty()) {
      throw std::runtime_error(fmt::format("Could not find {} in product registry (nothing matches "
                                           "all constraints). Contents are:\n{}\n",
                                           query,
                                           store_listing_for_error(store_)));
    }
    if (result.size() > 1) {
      throw std::runtime_error(
        fmt::format("Multiple matches for {} in product registry. Contents are:\n{}\n",
                    query,
                    store_listing_for_error(store_)));
    }
    return **result.begin();
  }
}
