#include "waveforms.hpp"

std::size_t demo::Waveforms::size() const { return waveforms.size(); }

demo::Waveforms::Waveforms(
  std::size_t n, double val, int run_id, int subrun_id, int spill_id, int apa_id) :
  waveforms(n, {val}), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id), apa_id(apa_id)
{
}

demo::Waveforms::Waveforms(Waveforms const& other) :
  waveforms(other.waveforms), spill_id(other.spill_id), apa_id(other.apa_id)
{
}

demo::Waveforms::Waveforms(Waveforms&& other) :
  waveforms(std::move(other.waveforms)), spill_id(other.spill_id), apa_id(other.apa_id)
{
}

demo::Waveforms& demo::Waveforms::operator=(Waveforms const& other)
{
  waveforms = other.waveforms;
  spill_id = other.spill_id;
  apa_id = other.apa_id;
  return *this;
}

demo::Waveforms& demo::Waveforms::operator=(Waveforms&& other)
{
  waveforms = std::move(other.waveforms);
  spill_id = other.spill_id;
  apa_id = other.apa_id;
  return *this;
}

demo::Waveforms::~Waveforms() {};
