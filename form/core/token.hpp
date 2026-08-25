// Copyright (C) 2025 ...

#ifndef FORM_CORE_TOKEN_HPP
#define FORM_CORE_TOKEN_HPP

#include "core/technology.hpp"

#include <cstdint>
#include <string>

/* @class token
 * @brief This class holds all the necessary information for reading of an object from a physical file.
 */
namespace form::detail::experimental {
  class token {
  public:
    /// Default constructor; a token with no id set (delegates to the placement-only constructor)
    token() : token("", "", {}) {}

    /// placement-only constructor; leaves the id unset (has_id() == false)
    token(std::string file_name, std::string container_name, technology::id technology);

    /// Fully-specified constructor; sets the 0-based row/entry id (has_id() == true)
    token(std::string file_name,
          std::string container_name,
          technology::id technology,
          std::uint64_t id);

    /// Access file name
    std::string const& file_name() const;
    /// Access container name
    std::string const& container_name() const;
    /// Access technology type
    technology::id technology() const;

    /// Access identifier/entry number (0-based row). Only meaningful when has_id() is true.
    std::uint64_t id() const;
    /// Whether an id has been set on this token
    bool has_id() const;

  private:
    /// Technology identifier
    technology::id technology_;
    /// File name
    std::string file_name_;
    /// Container name
    std::string container_name_;
    /// Identifier/entry number (0-based row)
    std::uint64_t id_;
    /// Whether id_ holds a valid, set value
    bool has_id_;
  };
} // namespace form::detail::experimental
#endif // FORM_CORE_TOKEN_HPP
