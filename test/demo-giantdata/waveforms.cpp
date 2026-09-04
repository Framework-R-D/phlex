#include "waveforms.hpp"

std::size_t demo::waveforms::size() const { return data.size(); }

demo::waveforms::waveforms(
  std::size_t n, double val, int run_id, int subrun_id, int spill_id, int apa_id) :
  data(n, {val}), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id), apa_id(apa_id)
{
}
