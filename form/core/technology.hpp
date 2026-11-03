#ifndef FORM_CORE_TECHNOLOGY_HPP
#define FORM_CORE_TECHNOLOGY_HPP

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

/* @file technology.hpp
 * @brief Storage technology identifiers and their configuration and on-disk names.
 *   - A storage technology, identified by a (major, minor) pair.
 *   - Configuration names and on-disk tokens are independent: configuration names are user-facing;
 *     on-disk tokens are part of the FORM persistent format. Neither is derived from the other.
 *   - Add technologies to detail::registry. Compile-time checks enforce the registry invariants.
 */

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

  // Backends: valid (major, minor) pairs. The numeric values are stable, but nothing writes
  // them to a file: a file names a technology by its on-disk token.
  inline constexpr id root_ttree{.major = major::root, .minor = 1};
  inline constexpr id root_rntuple{.major = major::root, .minor = 2};
  inline constexpr id hdf5{.major = major::hdf5, .minor = 1};

  namespace detail {

    struct registration {
      id technology;
      std::string_view configuration_name;
      std::string_view on_disk_token;
      bool is_implemented;
    };

    /// The single place a technology's spellings are declared. Nothing else may invent one.
    inline constexpr std::array registry{
      // Generic is not configurable but has an on-disk token.
      registration{.technology = id{},
                   .configuration_name = {},
                   .on_disk_token = "generic",
                   .is_implemented = true},
      registration{.technology = root_ttree,
                   .configuration_name = "ROOT_TTREE",
                   .on_disk_token = "root_ttree",
                   .is_implemented = true},
      registration{.technology = root_rntuple,
                   .configuration_name = "ROOT_RNTUPLE",
                   .on_disk_token = "root_rntuple",
                   .is_implemented = true},
      // Reserved for a future HDF5 backend.
      registration{.technology = hdf5,
                   .configuration_name = "HDF5",
                   .on_disk_token = "hdf5",
                   .is_implemented = false},
    };

    /// Every technology must be nameable on disk: a row without a token could not be written.
    consteval bool tokens_are_present()
    {
      return std::ranges::all_of(
        registry, [](registration const& entry) { return !entry.on_disk_token.empty(); });
    }

    /// Checks that technology IDs, tokens and non-empty configuration names are unique.
    consteval bool registry_is_unambiguous()
    {
      for (std::size_t i = 0; i < registry.size(); ++i) {
        for (std::size_t j = i + 1; j < registry.size(); ++j) {
          auto const& left = registry[i];
          auto const& right = registry[j];
          if (left.technology == right.technology) {
            return false;
          }
          if (left.on_disk_token == right.on_disk_token) {
            return false;
          }
          if (!left.configuration_name.empty() &&
              left.configuration_name == right.configuration_name) {
            return false;
          }
        }
      }
      return true;
    }

    static_assert(tokens_are_present(),
                  "every technology needs an on-disk token: it names the technology in a file");
    static_assert(registry_is_unambiguous(),
                  "two technologies claim the same id or the same spelling");

    constexpr registration const* lookup_by_id(id technology)
    {
      auto const found = std::ranges::find_if(registry, [technology](registration const& entry) {
        return entry.technology == technology;
      });
      return found == registry.end() ? nullptr : &*found;
    }

    constexpr registration const* lookup_by_configuration_name(std::string_view name)
    {
      if (name.empty()) {
        return nullptr;
      }
      auto const found = std::ranges::find_if(
        registry, [name](registration const& entry) { return entry.configuration_name == name; });
      return found == registry.end() ? nullptr : &*found;
    }

    // No empty guard is needed, unlike configuration names: every token is non-empty.
    constexpr registration const* lookup_by_on_disk_token(std::string_view token)
    {
      auto const found = std::ranges::find_if(
        registry, [token](registration const& entry) { return entry.on_disk_token == token; });
      return found == registry.end() ? nullptr : &*found;
    }

  } // namespace detail

  // ---- Configuration names: user-facing spellings, independent of on-disk tokens. ----

  inline std::string to_string(id tech)
  {
    auto const* const found = detail::lookup_by_id(tech);
    if (found == nullptr || found->configuration_name.empty()) {
      return "UNKNOWN";
    }
    return std::string{found->configuration_name};
  }

  /// Parses an exact configuration name; throws if unknown or unavailable.
  inline id from_string(std::string_view name)
  {
    auto const* const found = detail::lookup_by_configuration_name(name);
    if (found == nullptr) {
      if (detail::lookup_by_on_disk_token(name) != nullptr) {
        throw std::runtime_error("Unknown technology: " + std::string{name} +
                                 " (that is an on-disk token, not a configuration name)");
      }
      throw std::runtime_error("Unknown technology: " + std::string{name});
    }
    if (!found->is_implemented) {
      throw std::runtime_error("Technology '" + std::string{name} +
                               "' is recognized but not yet implemented");
    }
    return found->technology;
  }

  // ---- On-disk tokens: persistent identifiers written into FORM files. ----

  constexpr std::string_view on_disk_token(id tech)
  {
    auto const* const found = detail::lookup_by_id(tech);
    if (found == nullptr) {
      throw std::runtime_error("FORM: an unregistered technology has no on-disk token and "
                               "cannot be named in a file");
    }
    return found->on_disk_token;
  }

  constexpr std::optional<id> find_on_disk_token(std::string_view token)
  {
    auto const* const found = detail::lookup_by_on_disk_token(token);
    return found == nullptr ? std::nullopt : std::optional<id>{found->technology};
  }

  /// Parses an exact on-disk token; throws if unknown or unavailable.
  constexpr id from_on_disk_token(std::string_view token)
  {
    auto const* const found = detail::lookup_by_on_disk_token(token);
    if (found == nullptr) {
      throw std::runtime_error("Unknown FORM on-disk technology token: '" + std::string{token} +
                               "'");
    }
    if (!found->is_implemented) {
      throw std::runtime_error("FORM technology '" + std::string{token} +
                               "' is recorded in the file but is not implemented");
    }
    return found->technology;
  }

} // namespace form::technology

#endif // FORM_CORE_TECHNOLOGY_HPP
