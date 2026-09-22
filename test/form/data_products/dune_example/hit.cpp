// Copyright (C) 2025 ...

#include "hit.hpp"

#include <ostream>

namespace form::test {

  std::ostream& operator<<(std::ostream& os, plane_id const& id)
  {
    if (!id.is_valid) {
      return os << "plane_id{invalid}";
    }
    return os << "C:" << id.cryostat << " T:" << id.tpc << " P:" << id.plane;
  }

  std::ostream& operator<<(std::ostream& os, wire_id const& id)
  {
    if (!id.is_valid) {
      return os << "wire_id{invalid}";
    }
    return os << static_cast<plane_id const&>(id) << " W:" << id.number;
  }

  std::ostream& operator<<(std::ostream& os, hit const& h)
  {
    return os << "hit{channel=" << h.channel << ", ticks=[" << h.start_tick << ", " << h.end_tick
              << "], peak_time=" << h.peak_time << ", rms=" << h.rms
              << ", peak_amplitude=" << h.peak_amplitude << ", integral=" << h.integral
              << ", multiplicity=" << h.multiplicity << ", ndf=" << h.ndf << ", view=" << h.view
              << ", " << h.wire << "}";
  }

} // namespace form::test
