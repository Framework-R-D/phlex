// Copyright (C) 2025 ...

#ifndef TEST_FORM_DUNE_EXAMPLE_HIT_MAKER_HPP
#define TEST_FORM_DUNE_EXAMPLE_HIT_MAKER_HPP

#include "data_products/dune_example/hit.hpp"
#include "data_products/dune_example/hit_candidate.hpp"

#include <vector>

/* @file dune_example_hit_maker.hpp
 * @brief Deterministic fixture for the FORM navigation example.
 *
 * Produces multiple creators and invalid rows to exercise the wide navigation table layout.
 * Three hierarchies here:
 *   {spill, wire, roi}  cand_hit_standard         -> hitCandidates
 *                       find_hits_with_gaussians  -> roiHits
 *   {spill, wire}       fold_roi_hits             -> wireHits
 *   {spill}             fold_hits_into_vector     -> spillHits
 */

namespace form::test {

  /// Number of spills in the fixture.
  inline constexpr unsigned int number_of_spills = 2;

  /// Number of wires per spill.
  inline constexpr unsigned int number_of_wires = 3;

  /// Number of regions of interest for a wire; intentionally varies between 2 and 3.
  unsigned int rois_in(unsigned int spill, unsigned int wire);

  /// Whether the region produces fitted hits.
  bool fit_succeeded(unsigned int spill, unsigned int wire, unsigned int roi);

  /// Readout channel corresponding to a wire.
  unsigned int channel_of(unsigned int wire);

  /// Wire identifier corresponding to a wire number.
  wire_id wire_of(unsigned int wire);

  /// Candidate hits for a region.
  merged_hit_candidates candidates_in_roi(unsigned int spill, unsigned int wire, unsigned int roi);

  /// Fitted hits for a region; empty if the fit does not succeed.
  std::vector<hit> hits_in_roi(unsigned int spill, unsigned int wire, unsigned int roi);

  /// Fitted hits for a wire.
  std::vector<hit> hits_on_wire(unsigned int spill, unsigned int wire);

  /// Fitted hits for a spill.
  std::vector<hit> hits_in_spill(unsigned int spill);

} // namespace form::test

#endif // TEST_FORM_DUNE_EXAMPLE_HIT_MAKER_HPP
