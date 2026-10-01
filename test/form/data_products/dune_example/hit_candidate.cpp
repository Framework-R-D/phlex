// Copyright (C) 2025 ...

#include "hit_candidate.hpp"

#include <ostream>

namespace form::test {

  std::ostream& operator<<(std::ostream& os, hit_candidate const& candidate)
  {
    return os << "hit_candidate{ticks=[" << candidate.start_tick << ", " << candidate.stop_tick
              << "], center=" << candidate.hit_center << ", sigma=" << candidate.hit_sigma
              << ", height=" << candidate.hit_height << "}";
  }

} // namespace form::test
