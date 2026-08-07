#ifndef FORM_CORE_TECHNOLOGY_HPP
#define FORM_CORE_TECHNOLOGY_HPP

#include <compare>
#include <stdexcept>
#include <string>
#include <string_view>

/* A storage technology, identified by a (family, variant) pair */

namespace form::technology {

  // Storage family (ROOT, HDF5, ...)
  enum class Family : int {
    unknown = 0,
    root = 1,
    hdf5 = 2,
  };

  // Variant in family
  struct Id {
    Family family{Family::unknown};
    int variant{0};

    // Equality and ordering: ordering lets an Id be used as a std::map key
    constexpr auto operator<=>(Id const&) const = default;
  };

  // Backends: valid (family, variant) pairs, stable numeric values as a future Token may persist them
  inline constexpr Id ROOT_TTREE{Family::root, 1};
  inline constexpr Id ROOT_RNTUPLE{Family::root, 2};
  inline constexpr Id HDF5{Family::hdf5, 1};

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
      return HDF5;
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
