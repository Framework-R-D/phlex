#include "waveforms.hpp"
#include "log_record.hpp"
#include <cstddef>
#include <utility>

auto demo::Waveforms::size() const -> std::size_t { return waveforms.size(); }

demo::Waveforms::Waveforms(
  std::size_t n, double val, int run_id, int subrun_id, int spill_id, int apa_id) :
  waveforms(n, {val}), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id), apa_id(apa_id)
{
  log_record("wsctor", 0, 0, spill_id, apa_id, this, n, nullptr);
}

demo::Waveforms::Waveforms(Waveforms const& other) :
  waveforms(other.waveforms), spill_id(other.spill_id), apa_id(other.apa_id)
{
  log_record("wscopy", 0, 0, spill_id, apa_id, this, waveforms.size(), &other);
}

demo::Waveforms::Waveforms(Waveforms&& other) :
  waveforms(std::move(other.waveforms)), spill_id(other.spill_id), apa_id(other.apa_id)
{
  log_record("wsmove", 0, 0, spill_id, apa_id, this, waveforms.size(), &other);
}

auto demo::Waveforms::operator=(Waveforms const& other) -> demo::Waveforms&
{
  waveforms = other.waveforms;
  spill_id = other.spill_id;
  apa_id = other.apa_id;
  log_record("wscopy=", 0, 0, spill_id, apa_id, this, waveforms.size(), &other);
  return *this;
}

auto demo::Waveforms::operator=(Waveforms&& other) -> demo::Waveforms&
{
  waveforms = std::move(other.waveforms);
  spill_id = other.spill_id;
  apa_id = other.apa_id;
  log_record("wsmove=", 0, 0, spill_id, apa_id, this, waveforms.size(), &other);
  return *this;
}

demo::Waveforms::~Waveforms()
{
  log_record("wsdtor", 0, 0, spill_id, apa_id, this, waveforms.size(), nullptr);
};
