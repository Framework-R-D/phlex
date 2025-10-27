#include "waveform_generator_input.hpp"
#include "log_record.hpp"
#include <cstddef>

demo::WaveformGeneratorInput::WaveformGeneratorInput(std::size_t size,
                                                     std::size_t run_id,
                                                     std::size_t subrun_id,
                                                     std::size_t spill_id) :
  size(size), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id)
{
  log_record("wgictor", run_id, subrun_id, spill_id, -1, this, mysize(*this), nullptr);
}

demo::WaveformGeneratorInput::WaveformGeneratorInput(WaveformGeneratorInput const& other) :
  size(other.size), run_id(other.run_id), subrun_id(other.subrun_id), spill_id(other.spill_id)
{
  log_record("wgicopy", run_id, subrun_id, spill_id, -1, this, mysize(*this), &other);
}

demo::WaveformGeneratorInput::WaveformGeneratorInput(WaveformGeneratorInput&& other) :
  size(other.size), run_id(other.run_id), subrun_id(other.subrun_id), spill_id(other.spill_id)
{
  log_record("wgimove", run_id, subrun_id, spill_id, -1, this, mysize(*this), &other);
}

auto demo::WaveformGeneratorInput::operator=(
  WaveformGeneratorInput const& other) -> demo::WaveformGeneratorInput&
{
  log_record("wgicopy=", run_id, subrun_id, spill_id, -1, this, 0, &other);
  size = other.size;
  spill_id = other.spill_id;
  return *this;
}

auto demo::WaveformGeneratorInput::operator=(
  WaveformGeneratorInput&& other) -> demo::WaveformGeneratorInput&
{
  log_record("wgimove=", run_id, subrun_id, spill_id, -1, this, 0, &other);
  size = other.size;
  spill_id = other.spill_id;
  return *this;
}

demo::WaveformGeneratorInput::~WaveformGeneratorInput()
{
  log_record("wgidtor", run_id, subrun_id, spill_id, -1, this, mysize(*this), nullptr);
}
