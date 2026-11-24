// Copyright (C) 2025 ...

#include "navigation_catalog.hpp"

#include "core/technology.hpp"
#include "util/error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace form::detail::experimental;

namespace {

  // Technologies in the order a reader prefers them when several hold the same product.
  constexpr std::array read_preference{form::technology::root_rntuple,
                                       form::technology::root_ttree};

  std::string describe(product_key const& key)
  {
    return "product '" + key.product_name + "' from creator '" + key.creator + "' at stage '" +
           key.stage + "'";
  }

  std::string token_list(std::span<form::technology::id const> technologies)
  {
    std::string names;
    for (auto const technology : technologies) {
      if (!names.empty()) {
        names += ", ";
      }
      names += form::technology::on_disk_token(technology);
    }
    return names;
  }

}

namespace form::detail::experimental {

  std::optional<form::technology::id> choose_read_technology(
    std::span<form::technology::id const> holders)
  {
    if (holders.empty()) {
      return std::nullopt;
    }
    if (holders.size() == 1) {
      return holders.front();
    }
    auto const preferred =
      std::ranges::find_if(read_preference, [holders](form::technology::id const technology) {
        return std::ranges::find(holders, technology) != holders.end();
      });
    if (preferred == read_preference.end()) {
      error("no read preference among the technologies holding a product: " +
            token_list(holders));
    }
    return *preferred;
  }

  navigation_catalog::navigation_catalog(std::string file_name) : file_name_(std::move(file_name))
  {
  }

  void navigation_catalog::add(form::technology::id const technology,
                               product_dictionary const& dictionary)
  {
    std::string const token{form::technology::on_disk_token(technology)};
    if (!technologies_.insert(technology).second) {
      error("the product dictionary for '" + token + "' in file '" + file_name_ +
            "' was added twice");
    }

    auto const rows = dictionary.product_name.size();
    for (auto const* column : {&dictionary.creator,
                               &dictionary.stage,
                               &dictionary.container_name,
                               &dictionary.hierarchy_key,
                               &dictionary.navigation_container,
                               &dictionary.navigation_column}) {
      if (column->size() != rows) {
        error("the product dictionary for '" + token + "' in file '" + file_name_ +
              "' has columns of different lengths");
      }
    }

    for (std::size_t row = 0; row != rows; ++row) {
      product_key key{.product_name = dictionary.product_name[row],
                      .creator = dictionary.creator[row],
                      .stage = dictionary.stage[row]};
      auto& found = locations_[key];
      if (std::ranges::find(found, technology, &product_location::technology) != found.end()) {
        error("the product dictionary for '" + token + "' in file '" + file_name_ +
              "' lists " + describe(key) + " more than once");
      }
      found.push_back(product_location{.technology = technology,
                                       .container_name = dictionary.container_name[row],
                                       .hierarchy_key = dictionary.hierarchy_key[row],
                                       .navigation_container = dictionary.navigation_container[row],
                                       .navigation_column = dictionary.navigation_column[row]});
    }
  }

  std::vector<product_location> const& navigation_catalog::locations(product_key const& key) const
  {
    static std::vector<product_location> const none;
    auto const found = locations_.find(key);
    return found == locations_.end() ? none : found->second;
  }

  product_location const& navigation_catalog::select(product_key const& key) const
  {
    auto const& found = locations(key);
    if (found.empty()) {
      error("file '" + file_name_ + "' holds no " + describe(key));
    }

    std::vector<form::technology::id> holders;
    holders.reserve(found.size());
    for (auto const& location : found) {
      holders.push_back(location.technology);
    }
    auto const chosen = choose_read_technology(holders);
    return *std::ranges::find(found, *chosen, &product_location::technology);
  }

}
