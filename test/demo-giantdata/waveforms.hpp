// waveforms.hpp
#ifndef TEST_DEMO_GIANTDATA_WAVEFORMS_HPP
#define TEST_DEMO_GIANTDATA_WAVEFORMS_HPP

#include <array>
#include <vector>

namespace demo {

  struct waveform {
    // We should be set to the number of samples on a wire.
    std::array<double, static_cast<std::size_t>(3 * 1024)> samples;
  };

  struct waveforms {
    std::vector<waveform> data;
    int run_id;
    int subrun_id;
    int spill_id;
    int apa_id;

    std::size_t size() const;

    waveforms(std::size_t n, double val, int run_id, int subrun_id, int spill_id, int apa_id);
    waveforms(waveforms const& other);
    waveforms(waveforms&& other);

    // instrument copy assignment and move assignment
    waveforms& operator=(waveforms const& other);
    waveforms& operator=(waveforms&& other);

    ~waveforms();
  };

} // namespace demo
#endif // TEST_DEMO_GIANTDATA_WAVEFORMS_HPP
