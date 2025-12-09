#include <algorithm>

#include "user_algorithms.hpp"
#include "summed_clamped_waveforms.hpp"
#include "waveforms.hpp"

// This function is used to transform an input Waveforms object into an
// output Waveforms object. The output is a clamped version of the input.
demo::Waveforms demo::clampWaveforms(demo::Waveforms const& input)
{
  demo::Waveforms result(input);
  for (demo::Waveform& wf : result.waveforms) {
    for (double& x : wf.samples) {
      x = std::clamp(x, -10.0, 10.0);
    }
  }
  return result;
}

// This is the fold operator that will accumulate a SummedClampedWaveforms object.
void demo::accumulateSCW(demo::SummedClampedWaveforms& accumulator, demo::Waveforms const& wf)
{
  // This is the fold operator that will accumulate a SummedClampedWaveforms object.
  accumulator.size += wf.size();
  for (auto const& w : wf.waveforms) {
    for (double x : w.samples) {
      accumulator.sum += x;
    }
  }
}
