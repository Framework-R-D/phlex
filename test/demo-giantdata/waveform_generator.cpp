#include "waveforms.hpp"

#include "waveform_generator.hpp"
#include "waveform_generator_input.hpp"

#include <cstddef>

demo::waveform_generator::waveform_generator(wgi const& wgi) : maxsize_{wgi.size} {}

std::size_t demo::waveform_generator::initial_value() const { return 0; }

bool demo::waveform_generator::predicate(std::size_t made_so_far) const
{
  bool const result = made_so_far < maxsize_;
  return result;
}

std::pair<std::size_t, demo::waveforms> demo::waveform_generator::op(std::size_t made_so_far,
                                                                     std::size_t chunksize) const
{
  // How many waveforms should go into this chunk?
  std::size_t const newsize = std::min(chunksize, maxsize_ - made_so_far);
  auto result = std::make_pair(
    made_so_far + newsize, waveforms{newsize, static_cast<double>(made_so_far), -1, -1, -1, -1});
  return result;
}
