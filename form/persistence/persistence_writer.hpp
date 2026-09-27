// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
#define FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP

#include "core/container_naming.hpp"
#include "core/placement.hpp"
#include "core/product_identity.hpp"
#include "form/config.hpp"
#include "ipersistence_writer.hpp"
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
    token register_write(product_identity const& product,
                         placement const& plcmnt,
                         void const* data,
                         std::type_info const& type) override;
    void commit_place(placement const& plcmnt, cell_index const& cell) override;
    void finalize() override;

  private:
    /// Destination identified by file and technology; navigation is scoped to a place.
    using place_key = std::pair<std::string, technology::id>;

    /// A navigation stream is identified by (creator, stage).
    struct stream_key {
      std::string creator;
      std::string stage;

      auto operator<=>(stream_key const&) const = default;
    };

    /// A product write pending association with a data cell.
    struct pending_write {
      product_identity product;
      std::string container_name;
      std::uint64_t row{invalid_row_id};

      stream_key stream() const { return {.creator = product.creator, .stage = product.stage}; }
    };

    /// A navigation table is identified by the hierarchy it indexes, within one place.
    struct navigation_key {
      std::string file_name;
      form::technology::id technology;
      cell_hierarchy hierarchy;

      auto operator<=>(navigation_key const&) const = default;
    };

    /// A "wide" navigation table for one hierarchy.
    struct navigation_table {
      /// Streams contributing to this hierarchy, kept ordered for stable column order.
      std::set<stream_key> streams;

      /// Layer values -> stream -> physical row.
      /// A missing stream entry means that stream did not write the cell.
      std::map<std::vector<std::uint64_t>, std::map<stream_key, std::uint64_t>> rows;

      /// Physical column name -> what claimed it, so a clash can name both sides.
      std::map<std::string, std::string> column_sources;
    };

    /// One product dictionary entry describing how a product maps to its navigation column.
    struct dictionary_entry {
      std::string product_name;
      std::string creator;
      std::string stage;
      std::string container_name;
      std::string hierarchy_key;
      std::string navigation_container;
      std::string navigation_column;
    };

    /// Validated navigation update, not yet applied.
    struct staged_record {
      place_key place;
      navigation_key key;
      std::string table_name;
      std::vector<pending_write> pending;
      std::map<stream_key, std::uint64_t> row_by_stream;
      std::vector<std::uint64_t> layer_values;
      std::map<std::string, std::string> new_columns;
      bool claims_table_name{false};
    };

    /// Validate a record and compute its navigation update without mutating state.
    staged_record stage_navigation(placement const& plcmnt, cell_index const& cell);

    /// Apply a validated navigation update, consuming it.
    void apply_navigation(staged_record staged);

    /// Return one row per stream; reject inconsistent or duplicate writes.
    static std::map<stream_key, std::uint64_t> rows_by_stream(
      std::vector<pending_write> const& pending, cell_index const& cell);

    /// Add one dictionary row per (product, creator, stage, hierarchy) seen in this record.
    void record_dictionary_entries(place_key const& place,
                                   std::vector<pending_write> const& pending,
                                   cell_hierarchy const& hierarchy,
                                   technology::id tech);

    /// Throw if the row space is already owned by a different stream.
    void check_row_space_owner(placement const& plcmnt, stream_key const& stream) const;

    /// Throw if another hierarchy already claims this container name.
    /// Returns true if this hierarchy would be the first to claim it.
    bool check_table_name(navigation_key const& key, std::string const& table_name) const;

    /// Throw if this record would overwrite rows already recorded for the cell.
    static void check_rows_are_new(navigation_table const* table,
                                   cell_index const& cell,
                                   std::map<stream_key, std::uint64_t> const& row_by_stream);

    /// Stage a column claim, throwing if it conflicts with an existing or staged claim.
    static void stage_column(std::map<std::string, std::string>& staged,
                             navigation_table const* table,
                             std::string const& table_name,
                             std::string const& column,
                             std::string const& source);

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
    // Product container (file, name, technology) -> the "index" placement. The placement is
    // resolved when the product container is created and reused on each commit.
    std::map<std::tuple<std::string, std::string, technology::id>, placement> index_by_product_;

    // (file, technology, row space) -> owning stream. Each row space may have only one owner.
    std::map<std::tuple<std::string, technology::id, std::string>, stream_key> row_space_owners_;

    // Pending product writes grouped by destination place.
    std::map<place_key, std::vector<pending_write>> pending_by_place_;

    /// Navigation tables grouped by destination and hierarchy.
    std::map<navigation_key, navigation_table> navigation_tables_;

    /// (file, technology, container name) -> the hierarchy holding that name.
    std::map<std::tuple<std::string, technology::id, std::string>, cell_hierarchy>
      claimed_table_names_;

    /// Dictionary entries grouped by place and (product identity, hierarchy).
    std::map<place_key, std::map<std::pair<product_identity, cell_hierarchy>, dictionary_entry>>
      dictionaries_;
    bool finalized_{false};
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
