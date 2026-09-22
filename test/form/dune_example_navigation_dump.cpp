// Copyright (C) 2025 ...

// Prints and verifies the navigation layout produced by dune_example_writer.cpp.
// Fixture-specific checks are kept here; generic navigation checks are in navigation_check.hpp.

#include "core/technology.hpp"
#include "dune_example_hit_maker.hpp"
#include "navigation_check.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace form::test;

namespace {

  struct expected_shape {
    std::size_t roi_cells{};
    std::size_t wire_cells{};
    std::size_t spill_cells{};
    std::size_t unfitted_rois{};
  };

  expected_shape expected()
  {
    expected_shape shape;
    shape.spill_cells = number_of_spills;
    shape.wire_cells = std::size_t{number_of_spills} * number_of_wires;
    for (unsigned int spill = 0; spill != number_of_spills; ++spill) {
      for (unsigned int wire = 0; wire != number_of_wires; ++wire) {
        for (unsigned int roi = 0, rois = rois_in(spill, wire); roi != rois; ++roi) {
          ++shape.roi_cells;
          if (!fit_succeeded(spill, wire, roi)) {
            ++shape.unfitted_rois;
          }
        }
      }
    }
    return shape;
  }

  void check_hierarchies(checker& checks, layout const& found, std::string const& prefix)
  {
    auto const shape = expected();

    struct expectation {
      std::string table;
      std::vector<std::string> layer_columns;
      std::vector<std::string> creators;
      std::size_t rows;
    };

    for (auto const& [name, layer_columns, creators, rows] :
         {expectation{.table = prefix + "_cells_spill_wire_roi",
                      .layer_columns = {"spill", "wire", "roi"},
                      .creators = {"cand_hit_standard", "find_hits_with_gaussians"},
                      .rows = shape.roi_cells},
          expectation{.table = prefix + "_cells_spill_wire",
                      .layer_columns = {"spill", "wire"},
                      .creators = {"fold_roi_hits"},
                      .rows = shape.wire_cells},
          expectation{.table = prefix + "_cells_spill",
                      .layer_columns = {"spill"},
                      .creators = {"fold_hits_into_vector"},
                      .rows = shape.spill_cells}}) {
      auto const* table = found.table(name);
      checks.check(table != nullptr, "the file has a navigation table " + name);
      if (table == nullptr) {
        continue;
      }
      checks.check(table->layer_columns == layer_columns,
                   name + " carries the layers of its own hierarchy");
      checks.check(table->entries() == rows, name + " has one row per data cell");

      std::vector<std::string> names;
      names.reserve(table->creators.size());
      for (auto const& creator : table->creators) {
        names.push_back(creator.creator);
      }
      checks.check(names == creators, name + " has a column for each of its creators");
    }

    checks.check(found.tables.size() == 3, "three hierarchies give three navigation tables");
  }

  void check_absent_columns(checker& checks, column_reader& reader, std::string const& prefix)
  {
    auto const roi_table = prefix + "_cells_spill_wire_roi";
    auto const wire_table = prefix + "_cells_spill_wire";
    auto const spill_table = prefix + "_cells_spill";

    checks.check(!reader.has(column_of(wire_table, "roi")),
                 "the {spill, wire} table has no column for a layer outside its hierarchy");
    checks.check(!reader.has(column_of(spill_table, "wire")),
                 "the {spill} table has no column for a layer outside its hierarchy");
    checks.check(!reader.has(column_of(roi_table, "fold_roi_hits_row")),
                 "the wide table has no column for a creator that never wrote to it");
    checks.check(!reader.has(column_of(wire_table, "cand_hit_standard_row")),
                 "the {spill, wire} table has no column for a creator that never wrote to it");
  }

  void check_sparsity(checker& checks, layout const& found, std::string const& prefix)
  {
    auto const* table = found.table(prefix + "_cells_spill_wire_roi");
    if (table == nullptr) {
      return;
    }

    auto const* candidates = table->creator("cand_hit_standard");
    auto const* fitted = table->creator("find_hits_with_gaussians");
    checks.check(candidates != nullptr && fitted != nullptr,
                 "both creators of the {spill, wire, roi} hierarchy have a row column");
    if (candidates == nullptr || fitted == nullptr) {
      return;
    }

    checks.check(std::ranges::none_of(candidates->rows,
                                      [](std::uint64_t row) { return row == invalid_row_id; }),
                 "cand_hit_standard wrote every data cell of its hierarchy");
    auto const absent = std::ranges::count(fitted->rows, invalid_row_id);
    checks.check(std::cmp_equal(absent, expected().unfitted_rois),
                 "find_hits_with_gaussians is marked absent exactly where it wrote nothing");
  }

  void check_products(checker& checks, layout const& found, std::string const& prefix)
  {
    auto const roi_table = prefix + "_cells_spill_wire_roi";
    std::map<std::string, std::pair<std::string, std::string>> const wanted{
      {"hitCandidates", {"cand_hit_standard", roi_table}},
      {"roiHits", {"find_hits_with_gaussians", roi_table}},
      {"wireHits", {"fold_roi_hits", prefix + "_cells_spill_wire"}},
      {"spillHits", {"fold_hits_into_vector", prefix + "_cells_spill"}}};

    checks.check(found.products.size() == wanted.size(),
                 "the dictionary has one entry per product written");

    for (auto const& [label, expectation] : wanted) {
      auto const& [creator, table] = expectation;
      auto const* product = found.product(label);
      checks.check(product != nullptr, "the dictionary has an entry for " + label);
      if (product == nullptr) {
        continue;
      }
      checks.check(product->creator == creator, label + " names its creator");
      checks.check(product->navigation_container == table, label + " names its navigation table");
    }
  }

} // namespace

int main(int const argc, char* argv[])
{
  if (argc != 3) {
    std::cerr << "usage: dune_example_navigation_dump <file.root> <technology>\n";
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

  checker checks;
  column_reader reader{file_name, technology};

  auto const found = discover(checks, reader, tech_string);

  std::cout << "FORM navigation layout of " << file_name << " (" << tech_string << ")\n";
  print(found);

  check_layout(checks, reader, found);

  check_hierarchies(checks, found, prefix);
  check_absent_columns(checks, reader, prefix);
  check_sparsity(checks, found, prefix);
  check_products(checks, found, prefix);

  if (checks.failures() != 0) {
    std::cerr << '\n' << checks.failures() << " navigation check(s) failed\n";
    return 1;
  }
  std::cout << "\nnavigation layout verified\n";
  return 0;
}
