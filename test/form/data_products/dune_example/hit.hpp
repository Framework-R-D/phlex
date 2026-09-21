// Copyright (C) 2025 ...

#ifndef TEST_FORM_DATA_PRODUCTS_DUNE_EXAMPLE_HIT_HPP
#define TEST_FORM_DATA_PRODUCTS_DUNE_EXAMPLE_HIT_HPP

#include <compare>
#include <iosfwd>


namespace dune_example {

  /// A readout plane: cryostat, TPC, and plane number.
  struct plane_id {
    unsigned int cryostat{};
    unsigned int tpc{};
    unsigned int plane{};
    bool is_valid{false};

    std::strong_ordering operator<=>(plane_id const&) const = default;
  };

  /// A wire ID with a plane ID base, used to exercise base-class persistence.
  struct wire_id : plane_id {
    unsigned int number{};

    std::strong_ordering operator<=>(wire_id const&) const = default;
  };

  /// Build a valid wire ID.
  inline wire_id make_wire_id(unsigned int cryostat,
                              unsigned int tpc,
                              unsigned int plane,
                              unsigned int number)
  {
    return wire_id{{.cryostat = cryostat, .tpc = tpc, .plane = plane, .is_valid = true}, number};
  }

  /// A DUNE-shaped reconstructed hit used as a FORM test data product.
  struct hit {
    /// ID of the readout channel the hit was extracted from.
    unsigned int channel{};
    /// First TDC tick of the hit.
    int start_tick{};
    /// Last TDC tick of the hit.
    int end_tick{};
    /// Time of the signal peak, in tick units.
    float peak_time{};
    /// Uncertainty on the signal peak, in tick units.
    float sigma_peak_time{};
    /// RMS of the hit shape, in tick units.
    float rms{};
    /// Estimated amplitude of the hit at its peak, in ADC units.
    float peak_amplitude{};
    /// Uncertainty on the estimated peak amplitude, in ADC units.
    float sigma_peak_amplitude{};
    /// Sum of calibrated ADC counts over the region of interest.
    float roi_summed_adc{};
    /// Sum of calibrated ADC counts over the hit.
    float hit_summed_adc{};
    /// Integral under the calibrated signal waveform, in tick x ADC units.
    float integral{};
    /// Uncertainty on that integral, in ADC units.
    float sigma_integral{};
    /// How many hits this one could be sharing its signal window with.
    short int multiplicity{};
    /// Index of this hit among the multiplicity hits in the signal window.
    short int local_index{};
    /// Fit quality: chi-squared of the Gaussian fit.
    float goodness_of_fit{};
    /// Degrees of freedom in the determination of the hit shape.
    int ndf{};
    /// Plane projection this hit was measured in (geo::View_t as an int).
    int view{};
    /// Signal type of the plane (geo::SigType_t as an int).
    int signal_type{};
    /// Cryostat, TPC, plane, and wire number this hit sits on.
    wire_id wire;

    bool operator==(hit const&) const = default;
  };

  std::ostream& operator<<(std::ostream& os, plane_id const& id);
  std::ostream& operator<<(std::ostream& os, wire_id const& id);
  std::ostream& operator<<(std::ostream& os, hit const& h);

} // namespace dune_example

#endif // TEST_FORM_DATA_PRODUCTS_DUNE_EXAMPLE_HIT_HPP
