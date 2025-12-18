#ifndef TEST_DEMO_GIANTDATA_USER_ALGORITHMS_HPP
#define TEST_DEMO_GIANTDATA_USER_ALGORITHMS_HPP

#include "summed_clamped_waveforms.hpp"
#include "waveforms.hpp"

namespace demo {

  // This function is used to transform an input Waveforms object into an
  // output Waveforms object. The output is a clamped version of the input.
  Waveforms clampWaveforms(Waveforms const& input);

  // This is the fold operator that will accumulate a SummedClampedWaveforms object.
  void accumulateSCW(SummedClampedWaveforms& scw, Waveforms const& wf);
} // namespace demo

#endif // TEST_DEMO_GIANTDATA_USER_ALGORITHMS_HPP
