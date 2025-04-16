#ifndef test_demo_giantdata_waveform_generator_input_hpp
#define test_demo_giantdata_waveform_generator_input_hpp

#include <cstddef>

namespace demo {
  // This is the data product that our unfold node will receive from each spill.
  struct WaveformGeneratorInput {

    explicit WaveformGeneratorInput(std::size_t size = -1,
                                    std::size_t run_id = -1,
                                    std::size_t subrun_id = -1,
                                    std::size_t spill_id = -1);
    WaveformGeneratorInput(WaveformGeneratorInput const& other);
    WaveformGeneratorInput(WaveformGeneratorInput&& other);

    WaveformGeneratorInput& operator=(WaveformGeneratorInput const& other);
    WaveformGeneratorInput& operator=(WaveformGeneratorInput&& other);

    ~WaveformGeneratorInput();

    std::size_t size;
    std::size_t run_id;
    std::size_t subrun_id;
    std::size_t spill_id;
  };

  using WGI = WaveformGeneratorInput;
  inline std::size_t mysize(WaveformGeneratorInput const& w) { return sizeof(w); }
} // namespace demo

#endif // test_demo_giantdata_waveform_generator_input_hpp
