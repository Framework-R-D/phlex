#include "user_algorithms.hpp"
#include "log_record.hpp"
#include "summed_clamped_waveforms.hpp"
#include "waveforms.hpp"

// This function is used to transform an input Waveforms object into an
// output Waveforms object. The output is a clamped version of the input.
demo::waveforms demo::clamp_waveforms(demo::waveforms const& input,
                                      std::size_t run_id,
                                      std::size_t subrun_id,
                                      std::size_t spill_id,
                                      std::size_t apa_id)
{
  demo::log_record(
    "start_clamp", run_id, subrun_id, spill_id, apa_id, &input, input.size(), nullptr);
  demo::waveforms result(input);
  for (demo::waveform& wf : result.data) {
    for (double& x : wf.samples) {
      x = std::clamp(x, -10.0, 10.0);
    }
  }
  demo::log_record("end_clamp", run_id, subrun_id, spill_id, apa_id, &input, input.size(), &result);
  return result;
}

// This is the fold operator that will accumulate a summed_clamped_waveforms object.
void demo::accumulate_scw(demo::summed_clamped_waveforms& accumulator,
                          demo::waveforms const& wf,
                          std::size_t run_id,
                          std::size_t subrun_id,
                          std::size_t spill_id,
                          std::size_t apa_id)
{
  // This is the fold operator that will accumulate a summed_clamped_waveforms object.
  demo::log_record(
    "start_accSCW", run_id, subrun_id, spill_id, apa_id, &accumulator, wf.size(), &wf);
  accumulator.size += wf.size();
  for (auto const& w : wf.data) {
    for (double x : w.samples) {
      accumulator.sum += x;
    }
  }
  demo::log_record("end_accSCW", run_id, subrun_id, spill_id, apa_id, &accumulator, wf.size(), &wf);
}
