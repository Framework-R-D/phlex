#include "waveform_generator_input.hpp"
#include "log_record.hpp"

demo::waveform_generator_input::waveform_generator_input(std::size_t size,
                                                         std::size_t run_id,
                                                         std::size_t subrun_id,
                                                         std::size_t spill_id) :
  size(size), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id)
{
  log_record("wgictor", run_id, subrun_id, spill_id, -1, this, mysize(*this), nullptr);
}

demo::waveform_generator_input::waveform_generator_input(waveform_generator_input const& other) :
  size(other.size), run_id(other.run_id), subrun_id(other.subrun_id), spill_id(other.spill_id)
{
  log_record("wgicopy", run_id, subrun_id, spill_id, -1, this, mysize(*this), &other);
}

demo::waveform_generator_input::waveform_generator_input(waveform_generator_input&& other) :
  size(other.size), run_id(other.run_id), subrun_id(other.subrun_id), spill_id(other.spill_id)
{
  log_record("wgimove", run_id, subrun_id, spill_id, -1, this, mysize(*this), &other);
}

demo::waveform_generator_input& demo::waveform_generator_input::operator=(
  waveform_generator_input const& other)
{
  log_record("wgicopy=", run_id, subrun_id, spill_id, -1, this, 0, &other);
  size = other.size;
  spill_id = other.spill_id;
  return *this;
}

demo::waveform_generator_input& demo::waveform_generator_input::operator=(
  waveform_generator_input&& other)
{
  log_record("wgimove=", run_id, subrun_id, spill_id, -1, this, 0, &other);
  size = other.size;
  spill_id = other.spill_id;
  return *this;
}

demo::waveform_generator_input::~waveform_generator_input()
{
  log_record("wgidtor", run_id, subrun_id, spill_id, -1, this, mysize(*this), nullptr);
}
