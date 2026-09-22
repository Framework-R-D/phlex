// Copyright (C) 2025 ...

#ifndef TEST_FORM_NAVIGATION_CHECK_HPP
#define TEST_FORM_NAVIGATION_CHECK_HPP

#include "core/technology.hpp"
#include "storage/factories.hpp"
#include "storage/istorage.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

/* @file navigation_check.hpp
 * @brief Reads, prints and verifies a FORM file's navigation layout, whatever is in it.
 *
 * The layout is discovered from the product dictionary and read through FORM's
 * storage layer, so the checks are independent of the fixture and storage technology.
 */

namespace form::test {

  using form::detail::experimental::create_file;
  using form::detail::experimental::create_read_container;
  using form::detail::experimental::i_storage_file;
  using form::detail::experimental::i_storage_read_container;
  using form::detail::experimental::invalid_row_id;

  /// Counts failures instead of aborting, so one run reports everything that is wrong.
  class checker {
  public:
    void check(bool condition, std::string const& what)
    {
      if (!condition) {
        std::cerr << "FAILED: " << what << '\n';
        ++failures_;
      }
    }

    int failures() const { return failures_; }

  private:
    int failures_{0};
  };

  /// Extracts layer values from a data-cell index for the cross-check against the old index.
  inline std::optional<std::vector<std::uint64_t>> layer_values_of(std::string const& id)
  {
    if (id.size() < 2 || id.front() != '[' || id.back() != ']') {
      return std::nullopt;
    }
    std::vector<std::uint64_t> values;
    auto const body = id.substr(1, id.size() - 2);
    std::size_t start = 0;
    while (start < body.size()) {
      auto const comma = body.find(',', start);
      auto const field =
        body.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
      auto const colon = field.find(':');
      if (colon == std::string::npos) {
        return std::nullopt;
      }
      values.push_back(std::stoull(field.substr(colon + 1)));
      if (comma == std::string::npos) {
        break;
      }
      start = comma + 1;
    }
    return values;
  }

  inline std::string technology_token(std::string const& tech_string)
  {
    std::string name = tech_string;
    for (char& c : name) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return name;
  }

  inline std::string sanitized(std::string_view name)
  {
    std::string result;
    result.reserve(name.size());
    for (char const c : name) {
      auto const uc = static_cast<unsigned char>(c);
      result.push_back(std::isalnum(uc) != 0 || c == '_' ? c : '_');
    }
    return result;
  }

  inline std::string column_of(std::string const& table, std::string const& column)
  {
    return table + "/" + column;
  }

  class column_reader {
  public:
    column_reader(std::string const& file_name, form::technology::id technology) :
      file_(create_file(technology, file_name, 'i')), technology_(technology)
    {
    }

    std::optional<int> rows(std::string const& container)
    {
      try {
        return container_for(container)->entries();
      } catch (std::exception const&) {
        return std::nullopt;
      }
    }

    bool has(std::string const& container) { return rows(container).has_value(); }

    template <typename T>
    std::vector<T> column(std::string const& container)
    {
      std::vector<T> values;
      try {
        auto const cont = container_for(container);
        for (int row = 0, total = cont->entries(); row < total; ++row) {
          void const* raw = nullptr;
          if (!cont->read(row, &raw, typeid(T))) {
            break;
          }
          std::unique_ptr<T const> const owned{static_cast<T const*>(raw)};
          values.push_back(*owned);
        }
      } catch (std::exception const&) {
        values.clear();
      }
      return values;
    }

  private:
    std::shared_ptr<i_storage_read_container> container_for(std::string const& name)
    {
      if (auto const found = containers_.find(name); found != containers_.end()) {
        return found->second;
      }
      auto container = create_read_container(technology_, name);
      container->set_file(file_);
      return containers_.emplace(name, std::move(container)).first->second;
    }

    std::shared_ptr<i_storage_file> file_;
    form::technology::id technology_;
    std::map<std::string, std::shared_ptr<i_storage_read_container>> containers_;
  };

  struct creator_column {
    std::string creator;
    std::string column;
    std::vector<std::uint64_t> rows;
  };

  struct navigation_table {
    std::string name;
    std::string hierarchy_key;
    std::vector<std::string> layer_columns;
    std::vector<std::vector<std::uint64_t>> layers;
    std::vector<creator_column> creators;

    /// The shortest column, so that printing a malformed table is still safe. A table whose
    /// columns disagree is a failure, reported by check_layout rather than by crashing here.
    std::size_t entries() const
    {
      std::optional<std::size_t> shortest;
      auto const consider = [&shortest](std::size_t const size) {
        shortest = shortest ? std::min(*shortest, size) : size;
      };
      for (auto const& layer : layers) {
        consider(layer.size());
      }
      for (auto const& creator : creators) {
        consider(creator.rows.size());
      }
      return shortest.value_or(0);
    }

    std::vector<std::uint64_t> layer_key(std::size_t row) const
    {
      std::vector<std::uint64_t> key;
      key.reserve(layers.size());
      for (auto const& layer : layers) {
        key.push_back(layer[row]);
      }
      return key;
    }

    creator_column const* creator(std::string_view name) const
    {
      auto const found = std::ranges::find(creators, name, &creator_column::creator);
      return found == creators.end() ? nullptr : &*found;
    }
  };

  struct product_entry {
    std::string product_name;
    std::string creator;
    std::string container_name;
    std::string hierarchy_key;
    std::string navigation_container;
    std::string navigation_column;
  };

  struct layout {
    std::string technology_token;
    std::string dictionary;
    std::vector<product_entry> products;
    /// Ordered by table name, so the printout is stable.
    std::vector<navigation_table> tables;

    navigation_table const* table(std::string_view name) const
    {
      auto const found = std::ranges::find(tables, name, &navigation_table::name);
      return found == tables.end() ? nullptr : &*found;
    }

    product_entry const* product(std::string_view label) const
    {
      auto const found = std::ranges::find(products, label, &product_entry::product_name);
      return found == products.end() ? nullptr : &*found;
    }
  };

  namespace detail {

    inline std::vector<std::string> split_on(std::string const& text, char const separator)
    {
      std::vector<std::string> parts;
      std::size_t start = 0;
      while (true) {
        auto const at = text.find(separator, start);
        parts.push_back(text.substr(start, at == std::string::npos ? at : at - start));
        if (at == std::string::npos) {
          return parts;
        }
        start = at + 1;
      }
    }

    inline std::vector<std::string> layer_columns_of(checker& checks,
                                                     column_reader& reader,
                                                     std::string const& table,
                                                     std::string const& key)
    {
      if (key == "job") {
        return {}; // the job cell has no layers
      }
      auto candidates = split_on(key, '_');
      for (auto const& candidate : candidates) {
        if (!reader.has(column_of(table, candidate))) {
          checks.check(false,
                       std::format("hierarchy key '{}' resolves to the layer columns of {} (no "
                                   "column '{}'; a layer name containing '_'?)",
                                   key,
                                   table,
                                   candidate));
          return {};
        }
      }
      return candidates;
    }

    inline std::vector<std::string> table_headers(navigation_table const& table)
    {
      std::vector<std::string> headers;
      headers.reserve(table.layer_columns.size() + table.creators.size());
      headers.insert(headers.end(), table.layer_columns.begin(), table.layer_columns.end());
      for (auto const& creator : table.creators) {
        headers.push_back(creator.column);
      }
      return headers;
    }

    inline std::vector<std::string> table_row(navigation_table const& table, std::size_t const row)
    {
      std::vector<std::string> values;
      values.reserve(table.layers.size() + table.creators.size());
      for (auto const& layer : table.layers) {
        values.push_back(std::to_string(layer[row]));
      }
      for (auto const& creator : table.creators) {
        auto const id = creator.rows[row];
        values.push_back(id == invalid_row_id ? "-" : std::to_string(id));
      }
      return values;
    }

    inline std::vector<std::size_t> column_widths(navigation_table const& table,
                                                  std::vector<std::string> const& headers)
    {
      std::vector<std::size_t> widths;
      widths.reserve(headers.size());
      for (auto const& header : headers) {
        widths.push_back(header.size());
      }
      for (std::size_t row = 0; row != table.entries(); ++row) {
        auto const values = table_row(table, row);
        for (std::size_t col = 0; col != values.size() && col != widths.size(); ++col) {
          widths[col] = std::max(widths[col], values[col].size());
        }
      }
      return widths;
    }

    inline void print_row(std::vector<std::size_t> const& widths,
                          std::vector<std::string> const& values)
    {
      std::ostringstream line;
      for (std::size_t col = 0; col != values.size() && col != widths.size(); ++col) {
        line << (col == 0 ? "  " : " | ") << std::setw(static_cast<int>(widths[col]))
             << values[col];
      }
      std::cout << line.str() << '\n';
    }

    inline void check_table_shape(checker& checks, navigation_table const& table)
    {
      auto const entries = table.entries();
      checks.check(entries != 0, table.name + " is not empty");

      for (std::size_t col = 0; col != table.layers.size(); ++col) {
        checks.check(table.layers[col].size() == entries,
                     table.name + " column '" + table.layer_columns[col] +
                       "' has one value per row");
      }
      for (auto const& creator : table.creators) {
        checks.check(creator.rows.size() == entries,
                     table.name + " column '" + creator.column + "' has one value per row");
      }

      if (table.layers.empty()) {
        return;
      }
      for (std::size_t row = 1; row < entries; ++row) {
        checks.check(table.layer_key(row - 1) < table.layer_key(row),
                     table.name + " row " + std::to_string(row) +
                       " sorts after the one before it, so cells are sorted and unique");
      }
    }

    inline void check_against_creator_index(checker& checks,
                                            column_reader& reader,
                                            navigation_table const& table)
    {
      for (auto const& creator : table.creators) {
        auto const recorded_ids = reader.column<std::string>(column_of(creator.creator, "index"));
        checks.check(!recorded_ids.empty(),
                     "per-creator index '" + creator.creator + "/index' is readable");

        bool wrote_something = false;
        for (std::size_t row = 0; row != creator.rows.size(); ++row) {
          auto const id = creator.rows[row];
          if (id == invalid_row_id) {
            continue;
          }
          wrote_something = true;
          if (id >= recorded_ids.size()) {
            checks.check(false,
                         "navigation row for creator '" + creator.creator +
                           "' is within its index container");
            continue;
          }
          auto const recorded = layer_values_of(recorded_ids[id]);
          if (!recorded) {
            checks.check(false, "id at the navigated row has the expected data cell form");
            continue;
          }
          if (!table.layers.empty()) {
            checks.check(*recorded == table.layer_key(row),
                         "creator '" + creator.creator + "' row " + std::to_string(id) +
                           " holds the data cell navigation says it does");
          }
        }
        checks.check(wrote_something,
                     "creator '" + creator.creator + "' has a column in " + table.name +
                       " only because it wrote there");
      }
    }

    inline void check_dictionary(checker& checks, layout const& found)
    {
      for (auto const& product : found.products) {
        auto const what = "dictionary entry '" + product.product_name + "'";
        checks.check(product.container_name == product.creator + "/" + product.product_name,
                     what + " names its product container");
        checks.check(product.navigation_container ==
                       "nav_" + found.technology_token + "_cells_" + product.hierarchy_key,
                     what + " names the navigation table of its hierarchy");
        checks.check(product.navigation_column == sanitized(product.creator) + "_row",
                     what + " names its creator's row column");

        auto const* table = found.table(product.navigation_container);
        checks.check(table != nullptr, what + " points at a table that exists");
        if (table != nullptr) {
          checks.check(table->creator(product.creator) != nullptr,
                       what + " points at a table its creator has a column in");
        }
      }
    }

  } // namespace detail

  /// Read a file's whole navigation layout, discovering it from the product dictionary.
  inline layout discover(checker& checks, column_reader& reader, std::string const& tech_string)
  {
    layout found;
    found.technology_token = technology_token(tech_string);
    found.dictionary = "nav_" + found.technology_token + "_products";

    auto const names = reader.column<std::string>(column_of(found.dictionary, "product_name"));
    if (names.empty()) {
      checks.check(false,
                   "product dictionary '" + found.dictionary + "' is readable and not empty");
      return found;
    }

    auto const creators = reader.column<std::string>(column_of(found.dictionary, "creator"));
    auto const containers =
      reader.column<std::string>(column_of(found.dictionary, "container_name"));
    auto const keys = reader.column<std::string>(column_of(found.dictionary, "hierarchy_key"));
    auto const nav_containers =
      reader.column<std::string>(column_of(found.dictionary, "navigation_container"));
    auto const nav_columns =
      reader.column<std::string>(column_of(found.dictionary, "navigation_column"));

    auto const complete = creators.size() == names.size() && containers.size() == names.size() &&
                          keys.size() == names.size() && nav_containers.size() == names.size() &&
                          nav_columns.size() == names.size();
    checks.check(complete, "every dictionary column has one entry per product");
    if (!complete) {
      return found;
    }

    std::map<std::string, std::pair<std::string, std::map<std::string, std::string>>> by_table;
    for (std::size_t i = 0; i != names.size(); ++i) {
      found.products.push_back(product_entry{.product_name = names[i],
                                             .creator = creators[i],
                                             .container_name = containers[i],
                                             .hierarchy_key = keys[i],
                                             .navigation_container = nav_containers[i],
                                             .navigation_column = nav_columns[i]});
      auto& [key, columns] = by_table[nav_containers[i]];
      key = keys[i];
      columns[creators[i]] = nav_columns[i];
    }

    for (auto const& [name, contents] : by_table) {
      auto const& [key, columns] = contents;
      navigation_table table{.name = name, .hierarchy_key = key};
      table.layer_columns = detail::layer_columns_of(checks, reader, name, key);
      for (auto const& layer : table.layer_columns) {
        table.layers.push_back(reader.column<std::uint64_t>(column_of(name, layer)));
      }
      for (auto const& [creator, column] : columns) {
        table.creators.push_back(
          creator_column{.creator = creator,
                         .column = column,
                         .rows = reader.column<std::uint64_t>(column_of(name, column))});
      }
      found.tables.push_back(std::move(table));
    }

    return found;
  }

  inline void print_table(navigation_table const& table)
  {
    auto const headers = detail::table_headers(table);
    auto const widths = detail::column_widths(table, headers);

    std::cout << '\n'
              << table.name << "   hierarchy {" << table.hierarchy_key << "}, " << table.entries()
              << " rows, " << table.creators.size() << " creator(s)\n";
    detail::print_row(widths, headers);

    std::vector<std::string> rule;
    rule.reserve(widths.size());
    for (auto const width : widths) {
      rule.emplace_back(width, '-');
    }
    detail::print_row(widths, rule);

    for (std::size_t row = 0; row != table.entries(); ++row) {
      detail::print_row(widths, detail::table_row(table, row));
    }
  }

  /// Print the product dictionary, one block per product so that nothing has to be truncated.
  inline void print_dictionary(layout const& found)
  {
    std::cout << '\n' << found.dictionary << "   " << found.products.size() << " product(s)\n";
    for (auto const& product : found.products) {
      std::cout << '\n';
      auto const field = [](char const* name, std::string const& value) {
        std::cout << "  " << std::left << std::setw(22) << name << value << '\n' << std::right;
      };
      field("product_name", product.product_name);
      field("creator", product.creator);
      field("container_name", product.container_name);
      field("hierarchy_key", product.hierarchy_key);
      field("navigation_container", product.navigation_container);
      field("navigation_column", product.navigation_column);
    }
  }

  /// The whole layout: one table per hierarchy, then the dictionary.
  inline void print(layout const& found)
  {
    std::cout << "\nFORM navigation layout: " << found.tables.size() << " hierarchy table(s)"
              << " for technology '" << found.technology_token << "'.\n"
              << "A table with more than one creator column is a wide table: those creators\n"
              << "share a hierarchy, so they share its table.\n";
    for (auto const& table : found.tables) {
      print_table(table);
    }
    print_dictionary(found);
  }

  /// Invariants that hold for any FORM file, whatever hierarchies and creators it contains.
  inline void check_layout(checker& checks, column_reader& reader, layout const& found)
  {
    checks.check(!found.tables.empty(), "the file has at least one navigation table");

    for (auto const& table : found.tables) {
      detail::check_table_shape(checks, table);
      detail::check_against_creator_index(checks, reader, table);
    }

    detail::check_dictionary(checks, found);
  }

} // namespace form::test

#endif // TEST_FORM_NAVIGATION_CHECK_HPP
