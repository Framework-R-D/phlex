//A waveform is a collection of raw ADC values ordered in time paired with a channel.  This data product is intended exclusively for testing whether FORM can read a vector<waveform> with only a dictionary for waveform.
#ifndef FORM_TEST_WAVEFORM_HPP
#define FORM_TEST_WAVEFORM_HPP

#include <vector>
#include <cstdint>

namespace form::test {
  class waveform {
    public:
      waveform();
      waveform(std::vector<uint16_t> const& samples, int channel);
  
      std::vector<uint16_t> const& get_samples() const;
      int get_channel() const;
  
    private:
      std::vector<uint16_t> samples_;
      int channel_;
  };
}

#endif //FORM_TEST_WAVEFORM_HPP
