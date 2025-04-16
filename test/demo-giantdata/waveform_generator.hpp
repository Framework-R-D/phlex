#ifndef test_demo_giantdata_waveform_generator_hpp
#define test_demo_giantdata_waveform_generator_hpp

#include "log_record.hpp"
#include "waveform_generator_input.hpp"
#include "waveforms.hpp"

#include <cstddef>

namespace demo {
  // This is a class that provides unfold operation and a predicate to control
  // when the unfold is stopped.
  // Note that this class knows nothing about "spills" or "APAs". It does not
  // know where the values of maxsize and chunksize come from.
  class WaveformGenerator {
  public:
    // Create a WaveformGenerator that will generate waveforms exactly maxsize
    // waveforms. They will be spread across vectors each of size no more than
    //  chunksize.
    explicit WaveformGenerator(WGI const& wgi);
    WaveformGenerator(WaveformGenerator const&) = delete;
    WaveformGenerator(WaveformGenerator&&) = delete;
    WaveformGenerator& operator=(WaveformGenerator const&) = delete;
    WaveformGenerator& operator=(WaveformGenerator&&) = delete;
    ~WaveformGenerator();

    std::size_t initial_value() const;

    // When we have made at least as many waveforms as we have been asked to make
    // for this unfold, we are done. The predicate answers the question "should
    // we continue unfolding?"
    bool predicate(std::size_t made_so_far) const;

    // Generate the next chunk of waveforms, and update the count of how many
    // waveforms we have made so far.
    std::pair<std::size_t, Waveforms> op(std::size_t made_so_far, std::size_t chunksize) const;

  private:
    std::size_t maxsize_; // total number of waveforms to make for the unfold
    int spill_id_;        // the id of the spill this object will process
  }; // class WaveformGenerator
} // namespace demo
#endif // test_demo_giantdata_waveform_generator_hpp
