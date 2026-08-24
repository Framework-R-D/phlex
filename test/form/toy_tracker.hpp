//A toy PHLEX algorithm that takes a maximum number of TrackStarts to generate and generates random points for them.
#ifndef TEST_FORM_TOY_TRACKER_HPP
#define TEST_FORM_TOY_TRACKER_HPP
#include <cstdint>
#include <random>
#include <vector>

class track_start;

class toy_tracker {
public:
  explicit toy_tracker(int max_tracks);
  ~toy_tracker() = default;

  std::vector<track_start> operator()();

private:
  std::mt19937 gen_;
  std::uniform_int_distribution<int> size_dist_;
  std::uniform_real_distribution<float> value_dist_;
};

#endif // TEST_FORM_TOY_TRACKER_HPP
