// Copyright (C) 2025 ...

#include "navigation_catalog.hpp"

#include "core/container_naming.hpp"
#include "core/product_identity.hpp"
#include "core/technology.hpp"
#include "core/token.hpp"
#include "form/config.hpp"
#include "navigation_naming.hpp"
#include "storage/istorage.hpp"
#include "util/error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
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

  std::string describe(product_identity const& key)
  {
    return "product '" + key.label + "' from creator '" + key.creator + "' at stage '" + key.stage +
           "'";
  }

  // The member each column of product_dictionary_columns is read into, in the same order.
  constexpr std::array<std::vector<std::string> product_dictionary::*,
                       product_dictionary_columns.size()>
    dictionary_members{&product_dictionary::product_name,
                       &product_dictionary::creator,
                       &product_dictionary::stage,
                       &product_dictionary::container_name,
                       &product_dictionary::hierarchy_key,
                       &product_dictionary::navigation_container,
                       &product_dictionary::navigation_column};

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
    auto const* const preferred =
      std::ranges::find_if(read_preference, [holders](form::technology::id const technology) {
        return std::ranges::find(holders, technology) != holders.end();
      });
    if (preferred == read_preference.end()) {
      error("no read preference among the technologies holding a product: " + token_list(holders));
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
      product_identity const key{.creator = dictionary.creator[row],
                                 .stage = dictionary.stage[row],
                                 .label = dictionary.product_name[row]};
      auto& found = locations_[key];
      if (std::ranges::find(found, technology, &product_location::technology) != found.end()) {
        error("the product dictionary for '" + token + "' in file '" + file_name_ + "' lists " +
              describe(key) + " more than once");
      }
      found.push_back(product_location{.technology = technology,
                                       .container_name = dictionary.container_name[row],
                                       .hierarchy_key = dictionary.hierarchy_key[row],
                                       .navigation_container = dictionary.navigation_container[row],
                                       .navigation_column = dictionary.navigation_column[row]});
    }
  }

  std::vector<product_location> const& navigation_catalog::locations(
    product_identity const& key) const
  {
    static std::vector<product_location> const none;
    auto const found = locations_.find(key);
    return found == locations_.end() ? none : found->second;
  }

  product_location const& navigation_catalog::select(product_identity const& key) const
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
    // `found` is not empty, so a technology is always chosen.
    auto const chosen = choose_read_technology(holders).value_or(found.front().technology);
    return *std::ranges::find(found, chosen, &product_location::technology);
  }

  product_dictionary read_product_dictionary(
    i_storage_reader& storage,
    std::string const& file_name,
    form::technology::id const technology,
    form::experimental::config::tech_setting_config const& settings)
  {
    auto const table = navigation_dictionary_name(technology);
    product_dictionary dictionary;
    try {
      for (std::size_t column = 0; column != product_dictionary_columns.size(); ++column) {
        dictionary.*dictionary_members.at(column) = storage.read_string_column(
          token{
            file_name, build_full_label(table, product_dictionary_columns.at(column)), technology},
          settings);
      }
    } catch (std::exception const& e) {
      error("file '" + file_name + "' lists '" +
            std::string{form::technology::on_disk_token(technology)} +
            "' in its descriptor, but its product dictionary '" + table +
            "' cannot be read: " + e.what());
    }
    return dictionary;
  }

  navigation_catalog load_navigation_catalog(
    i_storage_reader& storage,
    std::string const& file_name,
    std::vector<form::technology::id> const& technologies,
    form::experimental::config::tech_setting_config const& settings)
  {
    navigation_catalog catalog{file_name};
    for (auto const technology : technologies) {
      catalog.add(technology, read_product_dictionary(storage, file_name, technology, settings));
    }
    return catalog;
  }

}
