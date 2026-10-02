// Verifies DUNE hit example output files, including navigation, products, FORM read-back, and
// ROOT key cycles
//
// usage: form_multiple_outputs_check <stage> (<file> <technology> <product,product,...>)...

#include "core/container_naming.hpp"
#include "core/technology.hpp"
#include "core/token.hpp"
#include "dune_example_hit_maker.hpp"
#include "form/config.hpp"
#include "navigation_check.hpp"
#include "storage/storage_reader.hpp"
#include "test/form/data_products/dune_example/hit_candidate.hpp"

#include <TFile.h>
#include <TKey.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace form::test;

namespace {

  std::set<std::string> split_products(std::string const& list)
  {
    std::set<std::string> products;
    std::istringstream stream{list};
    for (std::string product; std::getline(stream, product, ',');) {
      products.insert(product);
    }
    return products;
  }

  template <typename T>
  std::unique_ptr<T const> read_row(form::detail::experimental::storage_reader& storage,
                                    form::detail::experimental::token const& location)
  {
    void const* raw = nullptr;
    storage.read_container(
      location, &raw, typeid(T), form::experimental::config::tech_setting_config{});
    return std::unique_ptr<T const>{static_cast<T const*>(raw)};
  }

  bool matches_example(form::detail::experimental::storage_reader& storage,
                       form::detail::experimental::token const& location,
                       std::string const& label,
                       std::vector<std::uint64_t> const& layers)
  {
    auto const at = [&layers](std::size_t i) { return static_cast<unsigned int>(layers.at(i)); };
    auto const value = read_row<merged_hit_candidates>(storage, location);
    if (!value) {
      return false;
    }
    if (label == "hitCandidates") {
      return *value == candidates_in_roi(at(0), at(1), at(2));
    }
    if (label == "wireCandidates") {
      return *value == candidates_on_wire(at(0), at(1));
    }
    if (label == "spillCandidates") {
      return *value == candidates_in_spill(at(0));
    }
    return false;
  }

  // Reads every product cell using the row space for (technology, creator, stage).
  void check_read_back(checker& checks,
                       std::string const& file_name,
                       form::technology::id technology,
                       product_entry const& product)
  {
    using namespace form::detail::experimental;
    auto const what = std::format(
      "{} ({}): '{}'", file_name, form::technology::to_string(technology), product.product_name);
    auto const row_space = build_row_space_name(technology, product.creator, product.stage);
    token const index{file_name, build_full_label(row_space, "index"), technology};
    form::experimental::config::tech_setting_config const settings{};
    storage_reader storage;

    auto const ids = storage.list_indices(index, settings);
    checks.check(!ids.empty(), what + " has data cells");
    for (auto const& id : ids) {
      auto const layers = layer_values_of(id);
      if (!layers) {
        checks.check(false, std::format("{} has a parsable data cell {}", what, id));
        continue;
      }
      auto const row = static_cast<std::uint64_t>(storage.get_index(index, id, settings));
      token const location{
        file_name, build_full_label(row_space, product.product_name), technology, row};
      checks.check(matches_example(storage, location, product.product_name, *layers),
                   std::format("{} matches the example at {}", what, id));
    }
  }

  void check_output(checker& checks,
                    std::string const& stage,
                    std::string const& file_name,
                    std::string const& tech_string,
                    std::set<std::string> const& expected)
  {
    auto const technology = form::technology::from_string(tech_string);
    column_reader reader{file_name, technology};
    auto const found = discover(checks, reader, tech_string);
    check_layout(checks, reader, found);

    std::set<std::string> written;
    for (auto const& product : found.products) {
      written.insert(product.product_name);
      checks.check(product.stage == stage,
                   std::format("{}: '{}' was written at stage '{}', expected '{}'",
                               file_name,
                               product.product_name,
                               product.stage,
                               stage));
      check_read_back(checks, file_name, technology, product);
    }
    checks.check(written == expected, file_name + " holds exactly the products configured for it");
  }

  // Every object in the file must have been written exactly once.
  void check_single_cycles(checker& checks, std::string const& file_name)
  {
    std::unique_ptr<TFile> const file{TFile::Open(file_name.c_str(), "READ")};
    if (!file) {
      checks.check(false, file_name + " can be opened");
      return;
    }
    std::map<std::string, int> cycles;
    for (auto const* object : *file->GetListOfKeys()) {
      ++cycles[object->GetName()];
    }
    for (auto const& [name, count] : cycles) {
      checks.check(count == 1,
                   std::format("{}: '{}' has one cycle, not {}", file_name, name, count));
    }
  }

}

int main(int const argc, char* argv[])
{
  if (argc < 5 || (argc - 2) % 3 != 0) {
    std::cerr << "usage: form_multiple_outputs_check <stage> "
                 "(<file> <technology> <product,product,...>)...\n";
    return 1;
  }

  std::string const stage{argv[1]};
  checker checks;
  try {
    std::set<std::string> files;
    for (int arg = 2; arg < argc; arg += 3) {
      files.insert(argv[arg]);
    }
    for (auto const& file : files) {
      check_single_cycles(checks, file);
    }
    for (int arg = 2; arg < argc; arg += 3) {
      check_output(checks, stage, argv[arg], argv[arg + 1], split_products(argv[arg + 2]));
    }
  } catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }

  if (checks.failures() != 0) {
    std::cerr << checks.failures() << " check(s) failed\n";
    return 1;
  }
  std::cout << "outputs verified\n";
  return 0;
}
