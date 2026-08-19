// Copyright (C) 2025 ...

#ifndef FORM_CORE_TOKEN_HPP
#define FORM_CORE_TOKEN_HPP

#include "core/technology.hpp"

#include <cstdint>
#include <string>

/* @class Token
 * @brief This class holds all the necessary information for reading of an object from a physical file.
 */
namespace form::detail::experimental {
  class Token {
  public:
    /// Default constructor; a token with no id set (delegates to the placement-only constructor)
    Token() : Token("", "", {}) {}

    /// Placement-only constructor; leaves the id unset (hasId() == false)
    Token(std::string fileName, std::string containerName, technology::Id technology);

    /// Fully-specified constructor; sets the 0-based row/entry id (hasId() == true)
    Token(std::string fileName,
          std::string containerName,
          technology::Id technology,
          std::uint64_t id);

    /// Access file name
    std::string const& fileName() const;
    /// Access container name
    std::string const& containerName() const;
    /// Access technology type
    technology::Id technology() const;

    /// Access identifier/entry number (0-based row). Only meaningful when hasId() is true.
    std::uint64_t id() const;
    /// Whether an id has been set on this token
    bool hasId() const;

  private:
    /// Technology identifier
    technology::Id m_technology;
    /// File name
    std::string m_fileName;
    /// Container name
    std::string m_containerName;
    /// Identifier/entry number (0-based row)
    std::uint64_t m_id;
    /// Whether m_id holds a valid, set value
    bool m_hasId;
  };
} // namespace form::detail::experimental
#endif // FORM_CORE_TOKEN_HPP
