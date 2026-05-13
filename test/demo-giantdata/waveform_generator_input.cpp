#include "waveform_generator_input.hpp"

demo::WaveformGeneratorInput::WaveformGeneratorInput(std::size_t size,
                                                     std::size_t run_id,
                                                     std::size_t subrun_id,
                                                     std::size_t spill_id) :
  size(size), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id)
{
}
