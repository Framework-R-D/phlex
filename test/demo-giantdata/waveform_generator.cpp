#include "waveforms.hpp"

#include "log_record.hpp"
#include "waveform_generator.hpp"
#include "waveform_generator_input.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

demo::WaveformGenerator::WaveformGenerator(WGI const& wgi) :
  maxsize_{wgi.size}, spill_id_(wgi.spill_id)
{
  log_record("wgctor", -1, -1, spill_id_, -1, this, sizeof(*this), nullptr);
}

demo::WaveformGenerator::~WaveformGenerator()
{
  log_record("wgdtor", -1, -1, spill_id_, -1, this, sizeof(*this), nullptr);
}

auto demo::WaveformGenerator::initial_value() const -> std::size_t { return 0; }

auto demo::WaveformGenerator::predicate(std::size_t made_so_far) const -> bool
{
  log_record("start_pred", -1, -1, spill_id_, -1, this, made_so_far, nullptr);
  bool const result = made_so_far < maxsize_;
  log_record("end_pred", -1, -1, spill_id_, -1, this, made_so_far, nullptr);
  return result;
}

auto demo::WaveformGenerator::op(std::size_t made_so_far,
                                                                    std::size_t chunksize) const -> std::pair<std::size_t, demo::Waveforms>
{
  // How many waveforms should go into this chunk?
  log_record("start_op", -1, -1, spill_id_, -1, this, chunksize, nullptr);
  std::size_t const newsize = std::min(chunksize, maxsize_ - made_so_far);
  auto result =
    std::make_pair(made_so_far + newsize, Waveforms{newsize, 1.0 * made_so_far, -1, -1, -1, -1});
  log_record("end_op", -1, -1, spill_id_, -1, this, newsize, nullptr);
  return result;
}