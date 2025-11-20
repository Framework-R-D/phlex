#include "waveforms.hpp"
#include "log_record.hpp"

std::size_t demo::waveforms::size() const { return data.size(); }

demo::waveforms::waveforms(
  std::size_t n, double val, int run_id, int subrun_id, int spill_id, int apa_id) :
  data(n, {val}), run_id(run_id), subrun_id(subrun_id), spill_id(spill_id), apa_id(apa_id)
{
  log_record("wsctor", 0, 0, spill_id, apa_id, this, n, nullptr);
}

demo::waveforms::waveforms(waveforms const& other) :
  data(other.data), spill_id(other.spill_id), apa_id(other.apa_id)
{
  log_record("wscopy", 0, 0, spill_id, apa_id, this, data.size(), &other);
}

demo::waveforms::waveforms(waveforms&& other) :
  data(std::move(other.data)), spill_id(other.spill_id), apa_id(other.apa_id)
{
  log_record("wsmove", 0, 0, spill_id, apa_id, this, data.size(), &other);
}

demo::waveforms& demo::waveforms::operator=(waveforms const& other)
{
  data = other.data;
  spill_id = other.spill_id;
  apa_id = other.apa_id;
  log_record("wscopy=", 0, 0, spill_id, apa_id, this, data.size(), &other);
  return *this;
}

demo::waveforms& demo::waveforms::operator=(waveforms&& other)
{
  data = std::move(other.data);
  spill_id = other.spill_id;
  apa_id = other.apa_id;
  log_record("wsmove=", 0, 0, spill_id, apa_id, this, data.size(), &other);
  return *this;
}

demo::waveforms::~waveforms()
{
  log_record("wsdtor", 0, 0, spill_id, apa_id, this, data.size(), nullptr);
};
