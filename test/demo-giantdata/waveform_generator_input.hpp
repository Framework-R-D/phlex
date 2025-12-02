#ifndef TEST_DEMO_GIANTDATA_WAVEFORM_GENERATOR_INPUT_HPP
#define TEST_DEMO_GIANTDATA_WAVEFORM_GENERATOR_INPUT_HPP

#include <cstddef>

namespace demo {
  // This is the data product that our unfold node will receive from each spill.
  struct waveform_generator_input {

    explicit waveform_generator_input(std::size_t size = -1,
                                      std::size_t run_id = -1,
                                      std::size_t subrun_id = -1,
                                      std::size_t spill_id = -1);
    waveform_generator_input(waveform_generator_input const& other);
    waveform_generator_input(waveform_generator_input&& other);

    waveform_generator_input& operator=(waveform_generator_input const& other);
    waveform_generator_input& operator=(waveform_generator_input&& other);

    ~waveform_generator_input();

    std::size_t size;
    std::size_t run_id;
    std::size_t subrun_id;
    std::size_t spill_id;
  };

  using WGI = waveform_generator_input;
  inline std::size_t mysize(waveform_generator_input const& w) { return sizeof(w); }
} // namespace demo

#endif // TEST_DEMO_GIANTDATA_WAVEFORM_GENERATOR_INPUT_HPP
