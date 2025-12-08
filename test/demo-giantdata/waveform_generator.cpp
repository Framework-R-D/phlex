#include "waveforms.hpp"

#include "waveform_generator.hpp"
#include "waveform_generator_input.hpp"

#include <cstddef>

demo::WaveformGenerator::WaveformGenerator(WGI const& wgi) :
  maxsize_{wgi.size}
{
}

demo::WaveformGenerator::~WaveformGenerator()
{
}

std::size_t demo::WaveformGenerator::initial_value() const { return 0; }

bool demo::WaveformGenerator::predicate(std::size_t made_so_far) const
{
  bool const result = made_so_far < maxsize_;
  return result;
}

std::pair<std::size_t, demo::Waveforms> demo::WaveformGenerator::op(std::size_t made_so_far,
                                                                    std::size_t chunksize) const
{
  // How many waveforms should go into this chunk?
  std::size_t const newsize = std::min(chunksize, maxsize_ - made_so_far);
  auto result =
    std::make_pair(made_so_far + newsize, Waveforms{newsize, 1.0 * made_so_far, -1, -1, -1, -1});
  return result;
}