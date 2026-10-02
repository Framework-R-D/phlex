// Copyright (C) 2025 ...

#ifndef TEST_FORM_DATA_PRODUCTS_DUNE_EXAMPLE_HIT_CANDIDATE_HPP
#define TEST_FORM_DATA_PRODUCTS_DUNE_EXAMPLE_HIT_CANDIDATE_HPP

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace form::test {

  /// A candidate hit before Gaussian fitting.
  struct hit_candidate {
    std::size_t start_tick{};
    std::size_t stop_tick{};
    std::size_t max_tick{};
    std::size_t min_tick{};
    float max_derivative{};
    float min_derivative{};
    float hit_center{};
    float hit_sigma{};
    float hit_height{};

    bool operator==(hit_candidate const&) const = default;
  };

  using hit_candidate_vec = std::vector<hit_candidate>;

  using merged_hit_candidates = std::vector<hit_candidate_vec>;

  std::ostream& operator<<(std::ostream& os, hit_candidate const& candidate);

} // namespace form::test

#endif // TEST_FORM_DATA_PRODUCTS_DUNE_EXAMPLE_HIT_CANDIDATE_HPP
