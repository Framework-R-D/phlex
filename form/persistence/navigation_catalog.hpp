// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_NAVIGATION_CATALOG_HPP
#define FORM_PERSISTENCE_NAVIGATION_CATALOG_HPP

#include "core/technology.hpp"

#include <compare>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

/* @file navigation_catalog.hpp
 * @brief Where a file's products live, as recorded by its product dictionaries.
 *
 * Each technology in a file has its own product dictionary. The catalog gathers them, so a reader
 * can locate a product by what identifies it rather than by job configuration, and can choose a
 * technology when more than one holds the same product.
 */
namespace form::detail::experimental {

  /// Identifies a product the way its producer wrote it.
  struct product_key {
    std::string product_name;
    std::string creator;
    std::string stage;

    auto operator<=>(product_key const&) const = default;
  };

  /// Where one technology in a file holds a product.
  struct product_location {
    form::technology::id technology;
    std::string container_name;
    std::string hierarchy_key;
    std::string navigation_container;
    std::string navigation_column;

    bool operator==(product_location const&) const = default;
  };

  /// One technology's product dictionary, as its columns are read from the file.
  struct product_dictionary {
    std::vector<std::string> product_name;
    std::vector<std::string> creator;
    std::vector<std::string> stage;
    std::vector<std::string> container_name;
    std::vector<std::string> hierarchy_key;
    std::vector<std::string> navigation_container;
    std::vector<std::string> navigation_column;
  };

  /// The technology to read a product from, given every technology in the file that holds it.
  /// RNTuple is preferred over TTree. Returns std::nullopt if `holders` is empty.
  std::optional<form::technology::id> choose_read_technology(
    std::span<form::technology::id const> holders);

  /// Every product a file's dictionaries record, and where each technology holds it.
  class navigation_catalog {
  public:
    explicit navigation_catalog(std::string file_name);

    /// Records one technology's dictionary. Throws if its columns differ in length, if it lists a
    /// product twice, or if a dictionary for that technology was already added.
    void add(form::technology::id technology, product_dictionary const& dictionary);

    /// Every location holding the product, at most one per technology; empty if the file has
    /// none.
    std::vector<product_location> const& locations(product_key const& key) const;

    /// The location to read the product from. Throws if the file holds no such product.
    product_location const& select(product_key const& key) const;

  private:
    std::string file_name_;
    std::set<form::technology::id> technologies_;
    std::map<product_key, std::vector<product_location>> locations_;
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_NAVIGATION_CATALOG_HPP
