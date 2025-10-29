#include "waveforms.hpp"

#include "log_record.hpp"
#include "waveform_generator.hpp"
#include "waveform_generator_input.hpp"

#include <cstddef>

demo::waveform_generator::waveform_generator(WGI const& wgi) :
  maxsize_{wgi.size}, spill_id_(wgi.spill_id)
{
  log_record("wgctor", -1, -1, spill_id_, -1, this, sizeof(*this), nullptr);
}

demo::waveform_generator::~waveform_generator()
{
  log_record("wgdtor", -1, -1, spill_id_, -1, this, sizeof(*this), nullptr);
}

std::size_t demo::waveform_generator::initial_value() const { return 0; }

bool demo::waveform_generator::predicate(std::size_t made_so_far) const
{
  log_record("start_pred", -1, -1, spill_id_, -1, this, made_so_far, nullptr);
  bool const result = made_so_far < maxsize_;
  log_record("end_pred", -1, -1, spill_id_, -1, this, made_so_far, nullptr);
  return result;
}

std::pair<std::size_t, demo::waveforms> demo::waveform_generator::op(std::size_t made_so_far,
                                                                     std::size_t chunksize) const
{
  // How many waveforms should go into this chunk?
  log_record("start_op", -1, -1, spill_id_, -1, this, chunksize, nullptr);
  std::size_t const newsize = std::min(chunksize, maxsize_ - made_so_far);
  auto result =
    std::make_pair(made_so_far + newsize, waveforms{newsize, 1.0 * made_so_far, -1, -1, -1, -1});
  log_record("end_op", -1, -1, spill_id_, -1, this, newsize, nullptr);
  return result;
}
