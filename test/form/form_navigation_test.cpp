// Verifies one navigation container per hierarchy and technology, plus a product dictionary.
// The toy file contains two hierarchies: {event, segment} and {event}.
//
// Cross-check navigation rows against the existing per-creator index, which navigation is intended
// to replace.
//
// TTree only for now. RNTuple navigation is written through the same persistence code, but reading
// it back requires the RNTuple table reader from the read-path change.

#include "storage/istorage.hpp"

#include "TFile.h"
#include "TTree.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

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
      auto const token =
        body.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
      auto const colon = token.find(':');
      if (colon == std::string::npos) {
        return std::nullopt;
      }
      values.push_back(std::stoull(token.substr(colon + 1)));
      if (comma == std::string::npos) {
        break;
      }
      start = comma + 1;
    }
    return values;
  }

  TTree* get_tree(checker& checks, TFile& file, std::string const& name)
  {
    auto* tree = file.Get<TTree>(name.c_str());
    checks.check(tree != nullptr, "container '" + name + "' is present");
    return tree;
  }

  std::set<std::string> branch_names(TTree& tree)
  {
    std::set<std::string> names;
    for (auto const* obj : *tree.GetListOfBranches()) {
      names.insert(obj->GetName());
    }
    return names;
  }

  /// Read one string column. Only the requested branch is activated because per-creator containers
  /// also contain product branches.
  std::vector<std::string> read_string_column(TTree& tree, char const* column)
  {
    std::string value;
    auto* address = &value;
    tree.SetBranchStatus("*", false);
    tree.SetBranchStatus(column, true);
    tree.SetBranchAddress(column, &address);
    std::vector<std::string> values;
    for (Long64_t i = 0; i < tree.GetEntries(); ++i) {
      tree.GetEntry(i);
      values.push_back(value);
    }
    tree.ResetBranchAddresses();
    tree.SetBranchStatus("*", true);
    return values;
  }

  std::vector<std::uint64_t> read_uint_column(TTree& tree, char const* column)
  {
    std::uint64_t value = 0;
    tree.SetBranchStatus("*", false);
    tree.SetBranchStatus(column, true);
    tree.SetBranchAddress(column, &value);
    std::vector<std::uint64_t> values;
    for (Long64_t i = 0; i < tree.GetEntries(); ++i) {
      tree.GetEntry(i);
      values.push_back(value);
    }
    tree.ResetBranchAddresses();
    tree.SetBranchStatus("*", true);
    return values;
  }

  /// Read the per-creator index ids in row order.
  std::vector<std::string> read_creator_index(checker& checks,
                                              TFile& file,
                                              std::string const& creator)
  {
    auto* tree = file.Get<TTree>(creator.c_str());
    if (tree == nullptr) {
      checks.check(false, "per-creator container '" + creator + "' is present");
      return {};
    }
    return read_string_column(*tree, "index");
  }

  /// Cross-check that navigation's (cell, creator) -> row points to the same data cell recorded by
  /// the per-creator index.
  void cross_check(checker& checks,
                   TFile& file,
                   TTree& nav,
                   std::vector<std::string> const& layer_columns,
                   std::vector<std::string> const& creators)
  {
    std::vector<std::vector<std::uint64_t>> layers;
    layers.reserve(layer_columns.size());
    for (auto const& column : layer_columns) {
      layers.push_back(read_uint_column(nav, column.c_str()));
    }

    for (auto const& creator : creators) {
      auto const rows = read_uint_column(nav, (creator + "_row").c_str());
      auto const recorded_ids = read_creator_index(checks, file, creator);

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
  if (argc < 2) {
    std::cerr << "usage: form_navigation_test <file.root>\n";
    return 1;
  }

  std::unique_ptr<TFile> file{TFile::Open(argv[1], "READ")};
  if (!file || file->IsZombie()) {
    std::cerr << "could not open " << argv[1] << '\n';
    return 1;
  }

  checker checks;

  auto* segment_nav = get_tree(checks, *file, "nav_root_ttree_cells_event_segment");
  auto* event_nav = get_tree(checks, *file, "nav_root_ttree_cells_event");
  auto* dictionary = get_tree(checks, *file, "nav_root_ttree_products");
  if (segment_nav == nullptr || event_nav == nullptr || dictionary == nullptr) {
    return 1;
  }

  // Each hierarchy carries its own layer columns; navigation does not assume a fixed layer tuple.
  checks.check(branch_names(*segment_nav) ==
                 std::set<std::string>{"event", "segment", "Toy_Tracker_row"},
               "the {event, segment} table has its own layer columns");
  checks.check(branch_names(*event_nav) == std::set<std::string>{"event", "Toy_Tracker_Event_row"},
               "the {event} table has its own layer columns");

  // One row per data cell: 4 x 15 for {event, segment}, and 4 for {event}.
  checks.check(segment_nav->GetEntries() == 60,
               "the {event, segment} table has one row per data cell");
  checks.check(event_nav->GetEntries() == 4, "the {event} table has one row per data cell");

  // The dictionary maps each product to the navigation column that locates its data.
  auto const products = read_string_column(*dictionary, "product_name");
  auto const creators = read_string_column(*dictionary, "creator");
  auto const containers = read_string_column(*dictionary, "container_name");
  auto const hierarchies = read_string_column(*dictionary, "hierarchy_key");
  auto const nav_containers = read_string_column(*dictionary, "navigation_container");
  auto const nav_columns = read_string_column(*dictionary, "navigation_column");

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
    checks.check(nav_containers[i] == "nav_root_ttree_cells_event_segment",
                 "trackStart names its navigation table");
    checks.check(nav_columns[i] == "Toy_Tracker_row", "trackStart names its creator's row column");
  }
  checks.check(found_track_start, "the dictionary has an entry for trackStart");

  cross_check(checks, *file, *segment_nav, {"event", "segment"}, {"Toy_Tracker"});
  cross_check(checks, *file, *event_nav, {"event"}, {"Toy_Tracker_Event"});

  if (checks.failures() != 0) {
    std::cerr << checks.failures() << " navigation check(s) failed\n";
    return 1;
  }
  std::cout << "navigation layout verified\n";
  return 0;
}
