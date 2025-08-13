#include "toy_tracker.hpp"
#include "data_products/track_start.hpp"

ToyTracker::ToyTracker(int maxTracks) : m_maxTracks(maxTracks) {}

std::vector<TrackStart> ToyTracker::operator()()
{
  const int32_t npx = generateRandom() % m_maxTracks;
  std::vector<TrackStart> points(npx);
  for (int nelement = 0; nelement < npx; ++nelement) {
    points[nelement] = TrackStart(float(generateRandom()) / random_max,
                                  float(generateRandom()) / random_max,
                                  float(generateRandom()) / random_max);
  }

  return points;
}

int32_t ToyTracker::generateRandom()
{
  //Get a 32-bit random integer with even the lowest allowed precision of rand()
  int rand1 = rand() % 32768;
  int rand2 = rand() % 32768;
  return (rand1 * 32768 + rand2);
}
