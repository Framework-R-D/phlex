// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
#define FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP

#include "ipersistence_writer.hpp"

#include "core/container_naming.hpp"
#include "core/placement.hpp"
#include "form/config.hpp"
#include "storage/istorage.hpp"

#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <vector>

namespace form::detail::experimental {

  class persistence_writer : public i_persistence_writer {
  public:
    persistence_writer();
    // Test seam: inject a storage writer (e.g. a spy) instead of the default backend.
    explicit persistence_writer(std::unique_ptr<i_storage_writer> store_writer);
    ~persistence_writer() override = default;

    void configure_tech_settings(
      form::experimental::config::tech_setting_config const& tech_config_settings) override;

    void create_containers(
      std::vector<std::pair<placement, std::type_info const*>> const& containers) override;
    token register_write(placement const& plcmnt,
                         void const* data,
                         std::type_info const& type) override;
    void commit_place(placement const& plcmnt, cell_index const& cell) override;
    void finalize() override;

  private:
    /// Destination identified by file and technology; navigation is scoped to a place.
    using place_key = std::pair<std::string, technology::id>;

    /// A product write pending association with a data cell.
    struct pending_write {
      std::string creator;
      std::string label;
      std::string container_name;
      std::uint64_t row{invalid_row_id};
    };

    /// A navigation table is identified by the hierarchy it indexes, within one place.
    struct navigation_key {
      std::string file_name;
      form::technology::id technology;
      std::string hierarchy_key;

      auto operator<=>(navigation_key const&) const = default;
    };

    /// A "wide" navigation table for one hierarchy.
    struct navigation_table {
      /// Layer names for this hierarchy.
      std::vector<std::string> layer_names;

      /// Whether layer_names has been set by the first cell recorded here.
      /// Tracked separately because the job hierarchy has no layers.
      bool layers_set{false};

      /// Creators contributing to this hierarchy, kept ordered for stable column order.
      std::set<std::string> creators;

      /// Layer values -> creator -> physical row.
      /// A missing creator entry means that creator did not write the cell.
      std::map<std::vector<std::uint64_t>, std::map<std::string, std::uint64_t>> rows;
    };

    /// One product dictionary entry describing how a product maps to its navigation column.
    struct dictionary_entry {
      std::string product_name;
      std::string creator;
      std::string container_name;
      std::string hierarchy_key;
      std::string navigation_container;
      std::string navigation_column;
    };

    void record_navigation(placement const& plcmnt, cell_index const& cell);

    /// Return one row per creator, throwing if a creator's products use different rows.
    static std::map<std::string, std::uint64_t> rows_by_creator(
      std::vector<pending_write> const& pending, cell_index const& cell);

    /// Return the table for a key, initializing its layers on first use and throwing on mismatch.
    navigation_table& table_for(navigation_key const& key, cell_index const& cell);

    /// Add one dictionary row per (creator, product, hierarchy) seen in this record.
    void record_dictionary_entries(place_key const& place,
                                   std::vector<pending_write> const& pending,
                                   std::string const& hierarchy,
                                   technology::id tech);

    void write_navigation_tables();
    void write_product_dictionaries();

    /// Create one container for each table column and return their placements in column order.
    std::vector<placement> create_table_columns(std::string const& file_name,
                                                form::technology::id tech,
                                                std::string const& table_name,
                                                std::vector<std::string> const& columns,
                                                std::type_info const& type);

    std::unique_ptr<i_storage_writer> store_writer_;
    form::experimental::config::tech_setting_config tech_settings_;
    // Product container (file, name, technology) -> its per-creator "index" placement, resolved
    // once when the product container is created and reused on every commit.
    // Persistence owns the index.
    std::map<std::tuple<std::string, std::string, technology::id>, placement> index_by_product_;

    // Pending product writes grouped by destination place.
    std::map<place_key, std::vector<pending_write>> pending_by_place_;

    /// Navigation tables grouped by destination and hierarchy.
    std::map<navigation_key, navigation_table> navigation_tables_;

    /// Dictionary entries grouped by place and (creator, product, hierarchy).
    /// A product contributes one dictionary row per hierarchy.
    std::map<place_key,
             std::map<std::tuple<std::string, std::string, std::string>, dictionary_entry>>
      dictionaries_;
    bool finalized_{false};
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
