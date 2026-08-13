#ifndef FORM_CORE_TECHNOLOGY_HPP
#define FORM_CORE_TECHNOLOGY_HPP

#include <compare>
#include <stdexcept>
#include <string>
#include <string_view>

/* A storage technology, identified by a (major, minor) pair */

namespace form::technology {

  // Major storage type (ROOT, HDF5, ...)
  enum class Major {
    generic = 0, // no specific technology requested
    root = 1,
    hdf5 = 2,
  };

  // Minor variant within a Major (e.g. TTree vs RNTuple within ROOT)
  struct Id {
    Major major{Major::generic};
    int minor{0};

    // Exact ordering over (major, minor): lets an Id be a std::map key and drives backend dispatch
    constexpr auto operator<=>(Id const&) const = default;
  };

  // Backends: valid (major, minor) pairs, stable numeric values as a future Token may persist them
  inline constexpr Id ROOT_TTREE{Major::root, 1};
  inline constexpr Id ROOT_RNTUPLE{Major::root, 2};
  inline constexpr Id HDF5{Major::hdf5, 1};

  // Canonical string -> technology mapping: the single place a technology string is parsed, replacing the copies that used to live in each module/source/test
  inline Id from_string(std::string_view name)
  {
    if (name == "ROOT_TTREE") {
      return ROOT_TTREE;
    }
    if (name == "ROOT_RNTUPLE") {
      return ROOT_RNTUPLE;
    }
    if (name == "HDF5") {
      // HDF5 is a reserved technology but has no backend yet: reject it at parse time
      throw std::runtime_error("Technology 'HDF5' is recognized but not yet implemented");
    }
    throw std::runtime_error("Unknown technology: " + std::string(name));
  }

  // Canonical technology -> string mapping
  inline std::string to_string(Id tech)
  {
    if (tech == ROOT_TTREE) {
      return "ROOT_TTREE";
    }
    if (tech == ROOT_RNTUPLE) {
      return "ROOT_RNTUPLE";
    }
    if (tech == HDF5) {
      return "HDF5";
    }
    return "UNKNOWN";
  }

} // namespace form::technology

#endif // FORM_CORE_TECHNOLOGY_HPP
