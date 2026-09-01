#include <algorithm>

#include "summed_clamped_waveforms.hpp"
#include "user_algorithms.hpp"
#include "waveforms.hpp"

// This function is used to transform an input Waveforms object into an
// output Waveforms object. The output is a clamped version of the input.
demo::waveforms demo::clamp_waveforms(demo::waveforms const& input)
{
  demo::waveforms result(input);
  for (demo::waveform& wf : result.data) {
    for (double& x : wf.samples) {
      x = std::clamp(x, -10.0, 10.0);
    }
  }
  return result;
}

// This is the fold operator that will accumulate a SummedClampedWaveforms object.
void demo::accumulate_scw(demo::summed_clamped_waveforms& accumulator, demo::waveforms const& wf)
{
  // This is the fold operator that will accumulate a SummedClampedWaveforms object.
  accumulator.size += wf.size();
  for (auto const& w : wf.data) {
    for (double x : w.samples) {
      accumulator.sum += x;
    }
  }
}
