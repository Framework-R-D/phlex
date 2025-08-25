//A toy PHLEX algorithm that takes a maximum number of TrackStarts to generate and generates random points for them.
#ifndef __TOY_TRACKER_HPP__
#define __TOY_TRACKER_HPP__
#include <cstdint>
#include <vector>

class TrackStart;

class ToyTracker {
public:
  ToyTracker(int maxTracks);
  ~ToyTracker() = default;

  std::vector<TrackStart> operator()();

private:
  int m_maxTracks;
  int32_t generateRandom();
  int32_t random_max = 32768 * 32768;
};

#endif
