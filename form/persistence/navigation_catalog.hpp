// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_NAVIGATION_CATALOG_HPP
#define FORM_PERSISTENCE_NAVIGATION_CATALOG_HPP

#include "core/product_identity.hpp"
#include "core/technology.hpp"

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
namespace form::experimental::config {
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class i_storage_reader;

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

  /// Catalog of products recorded in a file's dictionaries, keyed by the writer's product_identity.
  /// Complete construction with add() before querying; locations() and select() return references.
  class navigation_catalog {
  public:
    explicit navigation_catalog(std::string file_name);

    /// Records one technology's dictionary. Throws if its columns differ in length, if it lists a
    /// product twice, or if a dictionary for that technology was already added.
    void add(form::technology::id technology, product_dictionary const& dictionary);

    /// Every location holding the product, at most one per technology; empty if the file has none.
    std::vector<product_location> const& locations(product_identity const& key) const;

    /// The location to read the product from. Throws if the file holds no such product.
    product_location const& select(product_identity const& key) const;

  private:
    std::string file_name_;
    std::set<form::technology::id> technologies_;
    std::map<product_identity, std::vector<product_location>> locations_;
  };

  /// Reads one technology's product dictionary from a file. Throws, naming the technology, if the
  /// dictionary cannot be read.
  product_dictionary read_product_dictionary(
    i_storage_reader& storage,
    std::string const& file_name,
    form::technology::id technology,
    form::experimental::config::tech_setting_config const& settings);

  /// Builds a file's catalog from the dictionaries of the technologies its descriptor lists.
  navigation_catalog load_navigation_catalog(
    i_storage_reader& storage,
    std::string const& file_name,
    std::vector<form::technology::id> const& technologies,
    form::experimental::config::tech_setting_config const& settings);

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_NAVIGATION_CATALOG_HPP
