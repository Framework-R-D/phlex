// Verifies one navigation container per hierarchy and technology, plus a product dictionary.
// The toy file contains two hierarchies: {event, segment} and {event}.
//
// Cross-check navigation rows against the existing per-creator index.
// Navigation is read through FORM's storage layer to exercise the same read interface for each
// supported technology.
// Container names are rebuilt here to keep the test independent of the production naming helpers.

#include "core/technology.hpp"
#include "storage/factories.hpp"
#include "storage/istorage.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

using form::detail::experimental::create_file;
using form::detail::experimental::create_read_container;
using form::detail::experimental::i_storage_file;
using form::detail::experimental::i_storage_read_container;
using form::detail::experimental::invalid_row_id;

namespace {

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

  /// Pull just the numbers out of a data cell's text, e.g. "[event:1, segment:2]" -> {1, 2}.
  /// Test-local parser used only to extract layer values for the cross-check; comparing values
  /// keeps the check independent of sanitized column names.
  std::optional<std::vector<std::uint64_t>> layer_values_of(std::string const& id)
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

  std::string technology_token(std::string const& tech_string)
  {
    std::string name = tech_string;
    for (char& c : name) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return name;
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

    template <typename T>
    std::vector<T> column(std::string const& container)
    {
      auto const cont = container_for(container);
      std::vector<T> values;
      for (int row = 0, total = cont->entries(); row < total; ++row) {
        void const* raw = nullptr;
        if (!cont->read(row, &raw, typeid(T))) {
          break;
        }
        std::unique_ptr<T const> const owned{static_cast<T const*>(raw)};
        values.push_back(*owned);
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

  std::string column_of(std::string const& table, std::string const& column)
  {
    return table + "/" + column;
  }

  /// Each hierarchy carries its own layer columns; navigation does not assume a fixed layer tuple.
  void check_table_columns(checker& checks,
                           column_reader& reader,
                           std::string const& segment_table,
                           std::string const& event_table)
  {
    checks.check(reader.rows(column_of(segment_table, "event")).has_value() &&
                   reader.rows(column_of(segment_table, "segment")).has_value() &&
                   reader.rows(column_of(segment_table, "Toy_Tracker_row")).has_value(),
                 "the {event, segment} table has its own layer and creator columns");
    checks.check(!reader.rows(column_of(segment_table, "Toy_Tracker_Event_row")).has_value(),
                 "the {event, segment} table has no column for a creator that never wrote to it");

    checks.check(reader.rows(column_of(event_table, "event")).has_value() &&
                   reader.rows(column_of(event_table, "Toy_Tracker_Event_row")).has_value(),
                 "the {event} table has its own layer and creator columns");
    checks.check(!reader.rows(column_of(event_table, "segment")).has_value(),
                 "the {event} table has no column for a layer outside its hierarchy");

    // One row per data cell: 4 x 15 for {event, segment}, and 4 for {event}.
    checks.check(reader.rows(column_of(segment_table, "event")) == 60,
                 "the {event, segment} table has one row per data cell");
    checks.check(reader.rows(column_of(event_table, "event")) == 4,
                 "the {event} table has one row per data cell");
  }

  /// The dictionary maps each product to the navigation column that locates its data.
  void check_dictionary(checker& checks,
                        column_reader& reader,
                        std::string const& dictionary,
                        std::string const& segment_table)
  {
    auto const products = reader.column<std::string>(column_of(dictionary, "product_name"));
    auto const creators = reader.column<std::string>(column_of(dictionary, "creator"));
    auto const containers = reader.column<std::string>(column_of(dictionary, "container_name"));
    auto const hierarchies = reader.column<std::string>(column_of(dictionary, "hierarchy_key"));
    auto const nav_containers =
      reader.column<std::string>(column_of(dictionary, "navigation_container"));
    auto const nav_columns = reader.column<std::string>(column_of(dictionary, "navigation_column"));

    bool found_track_start = false;
    for (std::size_t i = 0; i < products.size(); ++i) {
      if (products[i] != "trackStart" || creators[i] != "Toy_Tracker") {
        continue;
      }
      found_track_start = true;
      checks.check(containers[i] == "Toy_Tracker/trackStart",
                   "trackStart names its product container");
      checks.check(hierarchies[i] == "event_segment",
                   "trackStart belongs to the {event, segment} hierarchy");
      checks.check(nav_containers[i] == segment_table, "trackStart names its navigation table");
      checks.check(nav_columns[i] == "Toy_Tracker_row",
                   "trackStart names its creator's row column");
    }
    checks.check(found_track_start, "the dictionary has an entry for trackStart");
  }

  /// Cross-check that navigation's (cell, creator) -> row points to the same data cell recorded by
  /// the per-creator index.
  void cross_check(checker& checks,
                   column_reader& reader,
                   std::string const& table,
                   std::vector<std::string> const& layer_columns,
                   std::vector<std::string> const& creators)
  {
    std::vector<std::vector<std::uint64_t>> layers;
    layers.reserve(layer_columns.size());
    for (auto const& layer_column : layer_columns) {
      layers.push_back(reader.column<std::uint64_t>(column_of(table, layer_column)));
    }

    for (auto const& creator : creators) {
      auto const rows = reader.column<std::uint64_t>(column_of(table, creator + "_row"));
      auto const recorded_ids = reader.column<std::string>(column_of(creator, "index"));
      checks.check(!recorded_ids.empty(), "per-creator index '" + creator + "/index' is readable");

      for (std::size_t row = 0; row < rows.size(); ++row) {
        if (rows[row] == invalid_row_id) {
          continue; // this creator never wrote this data cell
        }
        if (rows[row] >= recorded_ids.size()) {
          checks.check(
            false, "navigation row for creator '" + creator + "' is within its index container");
          continue;
        }

        auto const recorded = layer_values_of(recorded_ids[rows[row]]);
        if (!recorded) {
          checks.check(false, "id at the navigated row has the expected data cell form");
          continue;
        }

        std::vector<std::uint64_t> expected;
        expected.reserve(layers.size());
        for (auto const& layer : layers) {
          expected.push_back(layer[row]);
        }
        checks.check(recorded == expected,
                     "creator '" + creator + "' row " + std::to_string(rows[row]) +
                       " holds the data cell navigation says it does");
      }
    }
  }
}

int main(int const argc, char const** argv)
{
  if (argc < 3) {
    std::cerr << "usage: form_navigation_test <file.root> <technology>\n";
    return 1;
  }

  std::string const file_name{argv[1]};
  std::string const tech_string{argv[2]};

  form::technology::id technology{};
  try {
    technology = form::technology::from_string(tech_string);
  } catch (std::exception const& e) {
    std::cerr << "unknown technology '" << tech_string << "': " << e.what() << '\n';
    return 1;
  }

  auto const prefix = "nav_" + technology_token(tech_string);
  auto const segment_table = prefix + "_cells_event_segment";
  auto const event_table = prefix + "_cells_event";
  auto const dictionary = prefix + "_products";

  checker checks;
  column_reader reader{file_name, technology};

  check_table_columns(checks, reader, segment_table, event_table);
  check_dictionary(checks, reader, dictionary, segment_table);
  cross_check(checks, reader, segment_table, {"event", "segment"}, {"Toy_Tracker"});
  cross_check(checks, reader, event_table, {"event"}, {"Toy_Tracker_Event"});

  if (checks.failures() != 0) {
    std::cerr << checks.failures() << " navigation check(s) failed\n";
    return 1;
  }
  std::cout << "navigation layout verified\n";
  return 0;
}
