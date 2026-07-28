//A toy PHLEX algorithm that takes a maximum number of TrackStarts to generate and generates random points for them.
#ifndef TEST_FORM_TOY_TRACKER_HPP
#define TEST_FORM_TOY_TRACKER_HPP
#include <cstdint>
#include <vector>
#include <random>

class TrackStart;

class ToyTracker {
public:
  explicit ToyTracker(int maxTracks);
  ~ToyTracker() = default;

  std::vector<TrackStart> operator()();

private:
  std::mt19937 m_gen;
  std::uniform_int_distribution<int> m_sizeDist;
  std::uniform_real_distribution<float> m_valueDist;
};

#endif // TEST_FORM_TOY_TRACKER_HPP
