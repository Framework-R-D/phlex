// Copyright (C) 2025 ...

// Phlex plugin for the DUNE hit example used by FORM output jobs.
//
// Only candidate hits are used because Phlex cannot yet derive the type_id of the fitted hit.

#include "dune_example_hit_maker.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <stdexcept>
#include <string>

using namespace phlex;
using namespace form::test;

namespace {

  unsigned int number_of(data_cell_index const& index)
  {
    return static_cast<unsigned int>(index.number());
  }

  unsigned int parent_number(data_cell_index const& index) { return number_of(*index.parent()); }

  merged_hit_candidates roi_candidates(data_cell_index const& roi)
  {
    auto const& wire = *roi.parent();
    return candidates_in_roi(parent_number(wire), number_of(wire), number_of(roi));
  }

  merged_hit_candidates wire_candidates(data_cell_index const& wire)
  {
    return candidates_on_wire(parent_number(wire), number_of(wire));
  }

  merged_hit_candidates spill_candidates(data_cell_index const& spill)
  {
    return candidates_in_spill(number_of(spill));
  }

  void check(handle<merged_hit_candidates> const& candidates,
             merged_hit_candidates const& expected,
             std::string const& what)
  {
    if (*candidates != expected) {
      throw std::runtime_error("dune_example_hits: " + what + " differs from the example at " +
                               candidates.data_cell_index().to_string());
    }
  }

}

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("cand_hit_standard", roi_candidates)
    .output_product("cand_hit_standard", "hitCandidates", "roi");
  s.provide("merge_roi_candidates", wire_candidates)
    .output_product("merge_roi_candidates", "wireCandidates", "wire");
  s.provide("merge_wire_candidates", spill_candidates)
    .output_product("merge_wire_candidates", "spillCandidates", "spill");
}

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.observe(
     "verify_hit_candidates",
     [](handle<merged_hit_candidates> const candidates) {
       check(candidates, roi_candidates(candidates.data_cell_index()), "hitCandidates");
     },
     concurrency::unlimited)
    .input_family(product_selector{.layer = "roi", .suffix = "hitCandidates"});
  m.observe(
     "verify_wire_candidates",
     [](handle<merged_hit_candidates> const candidates) {
       check(candidates, wire_candidates(candidates.data_cell_index()), "wireCandidates");
     },
     concurrency::unlimited)
    .input_family(product_selector{.layer = "wire", .suffix = "wireCandidates"});
  m.observe(
     "verify_spill_candidates",
     [](handle<merged_hit_candidates> const candidates) {
       check(candidates, spill_candidates(candidates.data_cell_index()), "spillCandidates");
     },
     concurrency::unlimited)
    .input_family(product_selector{.layer = "spill", .suffix = "spillCandidates"});
}
