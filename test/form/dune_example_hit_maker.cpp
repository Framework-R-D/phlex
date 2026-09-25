// Copyright (C) 2025 ...

#include "dune_example_hit_maker.hpp"

#include "test/form/data_products/dune_example/hit.hpp"
#include "test/form/data_products/dune_example/hit_candidate.hpp"

#include <utility>
#include <vector>

namespace {

  // Deterministic ordinal used to select regions whose fit fails.
  unsigned int roi_ordinal(unsigned int spill, unsigned int wire, unsigned int roi)
  {
    return (spill * 10) + (wire * 3) + roi;
  }

  // Number of candidate groups in a region.
  unsigned int groups_in_roi(unsigned int spill, unsigned int wire, unsigned int roi)
  {
    return 1 + ((spill + wire + roi) % 2);
  }

  unsigned int candidates_in_group(unsigned int wire, unsigned int roi, unsigned int group)
  {
    return 1 + ((wire + roi + group) % 3);
  }

  unsigned int roi_start_tick(unsigned int wire, unsigned int roi)
  {
    return 500 + (roi * 400) + (wire * 7);
  }

} // namespace

namespace form::test {

  unsigned int rois_in(unsigned int const spill, unsigned int const wire)
  {
    return 2 + ((spill + wire) % 2);
  }

  bool fit_succeeded(unsigned int const spill, unsigned int const wire, unsigned int const roi)
  {
    return roi_ordinal(spill, wire, roi) % 5 != 0;
  }

  unsigned int channel_of(unsigned int const wire) { return 2000 + wire; }

  wire_id wire_of(unsigned int const wire)
  {
    // Map the three fixture wires to U, V, and W planes.
    return make_wire_id(0, 0, wire % 3, wire);
  }

  merged_hit_candidates candidates_in_roi(unsigned int const spill,
                                          unsigned int const wire,
                                          unsigned int const roi)
  {
    unsigned int const start = roi_start_tick(wire, roi);

    merged_hit_candidates merged;
    unsigned int const groups = groups_in_roi(spill, wire, roi);
    merged.reserve(groups);

    for (unsigned int group = 0; group != groups; ++group) {
      hit_candidate_vec candidates;
      unsigned int const how_many = candidates_in_group(wire, roi, group);
      candidates.reserve(how_many);

      for (unsigned int index = 0; index != how_many; ++index) {
        unsigned int const center = start + 40 + (group * 120) + (index * 35);
        auto const nth = static_cast<float>(index);
        candidates.push_back(hit_candidate{.start_tick = center - 12,
                                           .stop_tick = center + 12,
                                           .max_tick = center - 4,
                                           .min_tick = center + 4,
                                           .max_derivative = 3.5F + nth,
                                           .min_derivative = -3.5F - nth,
                                           .hit_center = static_cast<float>(center),
                                           .hit_sigma = 4.25F,
                                           .hit_height = 18.0F + (6.0F * nth)});
      }
      merged.push_back(std::move(candidates));
    }

    return merged;
  }

  std::vector<hit> hits_in_roi(unsigned int const spill,
                               unsigned int const wire,
                               unsigned int const roi)
  {
    if (!fit_succeeded(spill, wire, roi)) {
      return {};
    }

    unsigned int const plane = wire % 3;
    int const signal_type = (plane == 2) ? 1 : 0;

    std::vector<hit> hits;
    for (auto const& group : candidates_in_roi(spill, wire, roi)) {
      auto const multiplicity = static_cast<short int>(group.size());
      short int local_index = 0;

      for (auto const& candidate : group) {
        float const center = candidate.hit_center;
        float const sigma = candidate.hit_sigma;
        float const height = candidate.hit_height;

        hits.push_back(hit{.channel = channel_of(wire),
                           .start_tick = static_cast<int>(candidate.start_tick),
                           .end_tick = static_cast<int>(candidate.stop_tick),
                           .peak_time = center,
                           .sigma_peak_time = 0.25F,
                           .rms = sigma,
                           .peak_amplitude = height,
                           .sigma_peak_amplitude = 0.5F,
                           .roi_summed_adc = height * 12.0F,
                           .hit_summed_adc = height * 9.0F,
                           .integral = height * sigma * 2.5F,
                           .sigma_integral = 1.5F,
                           .multiplicity = multiplicity,
                           .local_index = local_index,
                           .goodness_of_fit = 1.75F,
                           .ndf = 20,
                           .view = static_cast<int>(plane),
                           .signal_type = signal_type,
                           .wire = wire_of(wire)});
        ++local_index;
      }
    }

    return hits;
  }

  std::vector<hit> hits_on_wire(unsigned int const spill, unsigned int const wire)
  {
    std::vector<hit> hits;
    for (unsigned int roi = 0, rois = rois_in(spill, wire); roi != rois; ++roi) {
      auto const in_roi = hits_in_roi(spill, wire, roi);
      hits.insert(hits.end(), in_roi.begin(), in_roi.end());
    }
    return hits;
  }

  std::vector<hit> hits_in_spill(unsigned int const spill)
  {
    std::vector<hit> hits;
    for (unsigned int wire = 0; wire != number_of_wires; ++wire) {
      auto const on_wire = hits_on_wire(spill, wire);
      hits.insert(hits.end(), on_wire.begin(), on_wire.end());
    }
    return hits;
  }

} // namespace form::test
