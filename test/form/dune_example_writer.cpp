// Copyright (C) 2025 ...

#include "core/cell_index.hpp"
#include "core/technology.hpp"
#include "data_products/dune_example/hit.hpp"
#include "data_products/dune_example/hit_candidate.hpp"
#include "dune_example_hit_maker.hpp"
#include "form/form_writer.hpp"
#include "test_utils.hpp"

#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <typeinfo>
#include <vector>

/* @file dune_example_writer.cpp
 * @brief Writes the FORM navigation example file.
 *
 * A miniature GausHitFinder (see dune_example_hit_maker.hpp) over three hierarchies and four
 * creators, so that the resulting file contains three navigation tables, one of them wide. The
 * companion dune_example_navigation_dump.cpp reopens the file, prints those tables, and checks
 * them.
 *
 * This is the write half of the worked example in the FORM navigation documentation. It is
 * deliberately small enough that the whole navigation layout prints on one page.
 */

namespace {

  // Creators. Named after the design-3 GausHitFinder nodes in phlex-examples, so that the
  // navigation tables read like the ones a real workflow would produce.
  constexpr char const* cand_hit_standard = "cand_hit_standard";
  constexpr char const* find_hits_with_gaussians = "find_hits_with_gaussians";
  constexpr char const* fold_roi_hits = "fold_roi_hits";
  constexpr char const* fold_hits_into_vector = "fold_hits_into_vector";

  // Product labels.
  constexpr char const* hit_candidates_label = "hitCandidates";
  constexpr char const* roi_hits_label = "roiHits";
  constexpr char const* wire_hits_label = "wireHits";
  constexpr char const* spill_hits_label = "spillHits";

  form::detail::experimental::cell_index roi_cell(unsigned int const spill,
                                                  unsigned int const wire,
                                                  unsigned int const roi)
  {
    return {.id = std::format("[spill:{}, wire:{}, roi:{}]", spill, wire, roi),
            .hierarchy = {{"spill", "wire", "roi"}},
            .layer_values = {spill, wire, roi}};
  }

  form::detail::experimental::cell_index wire_cell(unsigned int const spill,
                                                   unsigned int const wire)
  {
    return {.id = std::format("[spill:{}, wire:{}]", spill, wire),
            .hierarchy = {{"spill", "wire"}},
            .layer_values = {spill, wire}};
  }

  form::detail::experimental::cell_index spill_cell(unsigned int const spill)
  {
    return {.id = std::format("[spill:{}]", spill),
            .hierarchy = {{"spill"}},
            .layer_values = {spill}};
  }

} // namespace

int main(int argc, char** argv)
{
  std::string const filename = (argc > 1) ? argv[1] : "dune_example_navigation.root";
  auto const technology = form::test::get_technology((argc > 2) ? argv[2] : "ROOT_TTREE");

  form::experimental::config::item_config config_items;
  config_items.add_item(hit_candidates_label, filename, technology);
  config_items.add_item(roi_hits_label, filename, technology);
  config_items.add_item(wire_hits_label, filename, technology);
  config_items.add_item(spill_hits_label, filename, technology);

  // dune_example::hit holds a dune_example::wire_id, which derives from plane_id, and
  // dune_example::merged_hit_candidates is a vector of vectors. RNTuple's native field mapping does
  // not cover either, so ask for the streamer field, as the toy fixture already does for
  // Toy_Tracker/trackStartPoints.
  form::experimental::config::tech_setting_config tech_config;
  for (auto const& container : {std::string(cand_hit_standard) + "/" + hit_candidates_label,
                                std::string(find_hits_with_gaussians) + "/" + roi_hits_label,
                                std::string(fold_roi_hits) + "/" + wire_hits_label,
                                std::string(fold_hits_into_vector) + "/" + spill_hits_label}) {
    tech_config.container_settings[form::technology::root_rntuple][container].emplace_back(
      "force_streamer_field", "true");
  }

  form::experimental::form_writer_interface form(config_items, tech_config);

  std::cout << "FORM navigation example: writing " << filename << '\n';

  unsigned int roi_cells = 0;
  unsigned int fitted_rois = 0;

  for (unsigned int spill = 0; spill != dune_example::fixture::number_of_spills; ++spill) {
    for (unsigned int wire = 0; wire != dune_example::fixture::number_of_wires; ++wire) {
      unsigned int const rois = dune_example::fixture::rois_in(spill, wire);
      for (unsigned int roi = 0; roi != rois; ++roi) {
        auto const cell = roi_cell(spill, wire, roi);
        ++roi_cells;

        // Creator 1 of 2 on this hierarchy: candidate finding, which writes every region.
        auto const candidates = dune_example::fixture::candidates_in_roi(spill, wire, roi);
        form::experimental::product_with_name const candidate_product{
          .label = hit_candidates_label,
          .data = &candidates,
          .type = &typeid(dune_example::merged_hit_candidates)};
        form.write(cand_hit_standard, cell, candidate_product);

        // Creator 2 of 2: the Gaussian fit, which writes nothing when no candidate survives.
        // Those regions are what put invalid_row_id into the wide navigation table.
        auto const hits = dune_example::fixture::hits_in_roi(spill, wire, roi);
        if (hits.empty()) {
          std::cout << "  " << cell.id << " no surviving candidate -- " << find_hits_with_gaussians
                    << " writes nothing\n";
          continue;
        }
        ++fitted_rois;
        form::experimental::product_with_name const roi_product{
          .label = roi_hits_label, .data = &hits, .type = &typeid(std::vector<dune_example::hit>)};
        form.write(find_hits_with_gaussians, cell, roi_product);
      }

      auto const wire_hits = dune_example::fixture::hits_on_wire(spill, wire);
      form::experimental::product_with_name const wire_product{
        .label = wire_hits_label,
        .data = &wire_hits,
        .type = &typeid(std::vector<dune_example::hit>)};
      form.write(fold_roi_hits, wire_cell(spill, wire), wire_product);
    }

    auto const spill_hits = dune_example::fixture::hits_in_spill(spill);
    form::experimental::product_with_name const spill_product{
      .label = spill_hits_label,
      .data = &spill_hits,
      .type = &typeid(std::vector<dune_example::hit>)};
    form.write(fold_hits_into_vector, spill_cell(spill), spill_product);
  }

  // Not strictly needed -- the destructor would do it -- but this is the example, so it shows
  // the call that writes the navigation tables.
  form.finalize();

  namespace fixture = dune_example::fixture;
  std::cout << "FORM navigation example: wrote " << roi_cells << " roi cells (" << fitted_rois
            << " fitted), " << (fixture::number_of_spills * fixture::number_of_wires)
            << " wire cells, " << fixture::number_of_spills << " spill cells\n";
  return 0;
}
