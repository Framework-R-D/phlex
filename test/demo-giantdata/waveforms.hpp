// waveforms.hpp
#ifndef TEST_DEMO_GIANTDATA_WAVEFORMS_HPP
#define TEST_DEMO_GIANTDATA_WAVEFORMS_HPP

#include <array>
#include <vector>

namespace demo {

  struct Waveform {
    // We should be set to the number of samples on a wire.
    std::array<double, 3uz * 1024> samples;
  };

  struct Waveforms {
    std::vector<Waveform> waveforms;
    int run_id;
    int subrun_id;
    int spill_id;
    int apa_id;

    std::size_t size() const;

    Waveforms(std::size_t n, double val, int run_id, int subrun_id, int spill_id, int apa_id);
  };

} // namespace demo
#endif // TEST_DEMO_GIANTDATA_WAVEFORMS_HPP
