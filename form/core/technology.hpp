#ifndef FORM_CORE_TECHNOLOGY_HPP
#define FORM_CORE_TECHNOLOGY_HPP

#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

/* A storage technology, identified by a (major, minor) pair */

namespace form::technology {

  // Major storage type (ROOT, HDF5, ...)
  enum class major : std::uint8_t {
    generic = 0, // no specific technology requested
    root = 1,
    hdf5 = 2,
  };

  // Minor variant within a major (e.g. TTree vs RNTuple within ROOT)
  struct id {
    // Member is named 'major' to keep the {major, minor} pair. The type must be
    // written QUALIFIED: GCC's -Wchanges-meaning rejects an unqualified
    // 'major major' (the name 'major' would mean the type, then the member),
    // while a qualified type is looked up in namespace scope and is fine.
    // clang accepts either, so the unqualified form breaks only in a GCC build.
    form::technology::major major{}; // value-initializes to major::generic (0)
    int minor{0};

    // Exact ordering over (major, minor): lets an id be a std::map key and drives backend dispatch
    constexpr auto operator<=>(id const&) const = default;
  };

  // Backends: valid (major, minor) pairs, stable numeric values as a future token may persist them
  inline constexpr id root_ttree{.major = major::root, .minor = 1};
  inline constexpr id root_rntuple{.major = major::root, .minor = 2};
  inline constexpr id hdf5{.major = major::hdf5, .minor = 1};

  // Canonical string -> technology mapping: the single place a technology string is parsed, replacing the copies that used to live in each module/source/test
  inline id from_string(std::string_view name)
  {
    if (name == "ROOT_TTREE") {
      return root_ttree;
    }
    if (name == "ROOT_RNTUPLE") {
      return root_rntuple;
    }
    if (name == "HDF5") {
      // HDF5 is a reserved technology but has no backend yet: reject it at parse time
      throw std::runtime_error("Technology 'HDF5' is recognized but not yet implemented");
    }
    throw std::runtime_error("Unknown technology: " + std::string(name));
  }

  // Canonical technology -> string mapping
  inline std::string to_string(id tech)
  {
    if (tech == root_ttree) {
      return "ROOT_TTREE";
    }
    if (tech == root_rntuple) {
      return "ROOT_RNTUPLE";
    }
    if (tech == hdf5) {
      return "HDF5";
    }
    return "UNKNOWN";
  }

} // namespace form::technology

#endif // FORM_CORE_TECHNOLOGY_HPP
