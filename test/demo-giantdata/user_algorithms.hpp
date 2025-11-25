#ifndef TEST_DEMO_GIANTDATA_USER_ALGORITHMS_HPP
#define TEST_DEMO_GIANTDATA_USER_ALGORITHMS_HPP

#include "summed_clamped_waveforms.hpp"
#include "waveforms.hpp"

namespace demo {

  // This function is used to transform an input Waveforms object into an
  // output Waveforms object. The output is a clamped version of the input.
  waveforms clamp_waveforms(waveforms const& input,
                            std::size_t run_id,
                            std::size_t subrun_id,
                            std::size_t spill_id,
                            std::size_t apa_id);

  // This is the fold operator that will accumulate a summed_clamped_waveforms object.
  void accumulate_scw(summed_clamped_waveforms& accumulator,
                      waveforms const& wf,
                      std::size_t run_id,
                      std::size_t subrun_id,
                      std::size_t spill_id,
                      std::size_t apa_id);

} // namespace demo

#endif // TEST_DEMO_GIANTDATA_USER_ALGORITHMS_HPP
