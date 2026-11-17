#include "waveform.hpp"

#include <vector>
#include <cstdint>

namespace form::test {
  waveform::waveform(): channel_(-1) {}
  
  waveform::waveform(std::vector<uint16_t> const& samples, int channel): samples_(samples), channel_(channel) {}

  std::vector<uint16_t> const& waveform::get_samples() const { return samples_; }

  int waveform::get_channel() const { return channel_; }
}
