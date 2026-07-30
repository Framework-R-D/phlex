#include "toy_tracker.hpp"
#include "data_products/track_start.hpp"

#include <algorithm>
#include <chrono>

ToyTracker::ToyTracker(int maxTracks) :
  m_gen(std::chrono::system_clock().now().time_since_epoch().count()),
  m_sizeDist(0, maxTracks - 1),
  m_valueDist(0, 1)
{
}

std::vector<TrackStart> ToyTracker::operator()()
{
  int32_t const npx = m_sizeDist(m_gen);
  std::vector<TrackStart> points;
  points.resize(npx);
  std::generate_n(std::back_inserter(points), npx, [this] {
    return TrackStart(m_valueDist(m_gen), m_valueDist(m_gen), m_valueDist(m_gen));
  });

  return points;
}
