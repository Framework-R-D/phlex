#include "toy_tracker.hpp"
#include "data_products/track_start.hpp"
#include <cstdint>
#include <cstdlib>
#include <vector>

ToyTracker::ToyTracker(int maxTracks) : m_maxTracks(maxTracks) {}

auto ToyTracker::operator()() -> std::vector<TrackStart>
{
  int32_t const npx = generateRandom() % m_maxTracks;
  std::vector<TrackStart> points(npx);
  for (int nelement = 0; nelement < npx; ++nelement) {
    points[nelement] = TrackStart(float(generateRandom()) / random_max,
                                  float(generateRandom()) / random_max,
                                  float(generateRandom()) / random_max);
  }

  return points;
}

auto ToyTracker::generateRandom() -> int32_t
{
  //Get a 32-bit random integer with even the lowest allowed precision of rand()
  int rand1 = rand() % 32768;
  int rand2 = rand() % 32768;
  return (rand1 * 32768 + rand2);
}
