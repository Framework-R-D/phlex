// Verifies one navigation container per hierarchy and technology, plus a product dictionary.
// The toy file contains two hierarchies: {event, segment} and {event}.
//
// Generic navigation checks are in navigation_check.hpp; this file checks the toy fixture.

#include "core/technology.hpp"
#include "navigation_check.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

using namespace form::test;

namespace {

  /// Fixture-specific negative checks that discovery cannot express.
  void check_toy_columns(checker& checks,
                         column_reader& reader,
                         std::string const& segment_table,
                         std::string const& event_table)
  {
    checks.check(!reader.has(column_of(segment_table, "Toy_Tracker_Event_row")),
                 "the {event, segment} table has no column for a creator that never wrote to it");
    checks.check(!reader.has(column_of(event_table, "segment")),
                 "the {event} table has no column for a layer outside its hierarchy");
  }

  void check_toy_shape(checker& checks,
                       layout const& found,
                       std::string const& segment_table,
                       std::string const& event_table)
  {
    checks.check(found.tables.size() == 2, "the toy file has one table per hierarchy, and two");

    struct expectation {
      std::string table;
      std::vector<std::string> layer_columns;
      std::vector<std::string> creators;
      std::size_t rows;
    };

    // 4 events, 15 segments each, from one creator; then 4 events from the other.
    for (auto const& [name, layer_columns, creators, rows] :
         {expectation{.table = segment_table,
                      .layer_columns = {"event", "segment"},
                      .creators = {"Toy_Tracker"},
                      .rows = 60},
          expectation{.table = event_table,
                      .layer_columns = {"event"},
                      .creators = {"Toy_Tracker_Event"},
                      .rows = 4}}) {
      auto const* table = found.table(name);
      checks.check(table != nullptr, "the file has a navigation table " + name);
      if (table == nullptr) {
        continue;
      }
      checks.check(table->layer_columns == layer_columns, name + " carries its own layer columns");
      checks.check(table->entries() == rows, name + " has one row per data cell");

      std::vector<std::string> names;
      names.reserve(table->creators.size());
      for (auto const& creator : table->creators) {
        names.push_back(creator.creator);
      }
      checks.check(names == creators, name + " has a column for each of its creators");
    }
  }

  /// The dictionary maps each product to the navigation column that locates its data.
  void check_toy_dictionary(checker& checks, layout const& found, std::string const& segment_table)
  {
    auto const* track_start = found.product("trackStart");
    checks.check(track_start != nullptr, "the dictionary has an entry for trackStart");
    if (track_start == nullptr) {
      return;
    }
    checks.check(track_start->creator == "Toy_Tracker", "trackStart names its creator");
    checks.check(track_start->container_name == "Toy_Tracker/trackStart",
                 "trackStart names its product container");
    checks.check(track_start->hierarchy_key == "event_segment",
                 "trackStart belongs to the {event, segment} hierarchy");
    checks.check(track_start->navigation_container == segment_table,
                 "trackStart names its navigation table");
    checks.check(track_start->navigation_column == "Toy_Tracker_row",
                 "trackStart names its creator's row column");
  }

}

int main(int const argc, char* argv[])
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

  checker checks;
  column_reader reader{file_name, technology};

  auto const found = discover(checks, reader, tech_string);
  print(found);

  check_layout(checks, reader, found);

  check_toy_shape(checks, found, segment_table, event_table);
  check_toy_columns(checks, reader, segment_table, event_table);
  check_toy_dictionary(checks, found, segment_table);

  if (checks.failures() != 0) {
    std::cerr << checks.failures() << " navigation check(s) failed\n";
    return 1;
  }
  std::cout << "\nnavigation layout verified\n";
  return 0;
}
