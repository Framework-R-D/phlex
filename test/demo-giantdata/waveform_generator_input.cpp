#include "waveform_generator_input.hpp"

demo::WaveformGeneratorInput::WaveformGeneratorInput(std::size_t size,
                                                     std::size_t run_id,
                                                     std::size_t subrun_id,
                                                     std::size_t spill_id) :
  size(size), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id)
{
}

demo::WaveformGeneratorInput::WaveformGeneratorInput(WaveformGeneratorInput const& other) :
  size(other.size), run_id(other.run_id), subrun_id(other.subrun_id), spill_id(other.spill_id)
{
}

demo::WaveformGeneratorInput::WaveformGeneratorInput(WaveformGeneratorInput&& other) :
  size(other.size), run_id(other.run_id), subrun_id(other.subrun_id), spill_id(other.spill_id)
{
}

demo::WaveformGeneratorInput& demo::WaveformGeneratorInput::operator=(
  WaveformGeneratorInput const& other)
{
  size = other.size;
  spill_id = other.spill_id;
  return *this;
}

demo::WaveformGeneratorInput& demo::WaveformGeneratorInput::operator=(
  WaveformGeneratorInput&& other)
{
  size = other.size;
  spill_id = other.spill_id;
  return *this;
}

demo::WaveformGeneratorInput::~WaveformGeneratorInput()
{
}
