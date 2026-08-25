#include "toy_tracker.hpp"
#include "data_products/track_start.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

toy_tracker::toy_tracker(int max_tracks) :
  gen_(std::chrono::system_clock().now().time_since_epoch().count()),
  size_dist_(0, max_tracks - 1),
  value_dist_(0, 1)
{
  assert(max_tracks > 1);
}

std::vector<track_start> toy_tracker::operator()()
{
  int32_t const npx = size_dist_(gen_);
  std::vector<track_start> points;
  points.reserve(npx);
  std::generate_n(std::back_inserter(points), npx, [this] {
    return track_start(value_dist_(gen_), value_dist_(gen_), value_dist_(gen_));
  });

  return points;
}
