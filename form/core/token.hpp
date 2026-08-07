// Copyright (C) 2025 ...

#ifndef FORM_CORE_TOKEN_HPP
#define FORM_CORE_TOKEN_HPP

#include "core/technology.hpp"

#include <string>

/* @class Token
 * @brief This class holds all the necessary information for reading of an object from a physical file.
 */
namespace form::detail::experimental {
  class Token {
  public:
    /// Default constructor; delegates to the named constructor so the -1 sentinel for id is defined once
    Token() : Token("", "", {}) {}

    /// Named constructor; id defaults to -1 as a "not set" sentinel
    Token(std::string fileName, std::string containerName, technology::Id technology, int id = -1);

    /// Access file name
    std::string const& fileName() const;
    /// Access container name
    std::string const& containerName() const;
    /// Access technology type
    technology::Id technology() const;

    /// Access identifier/entry number
    int id() const;

  private:
    /// Technology identifier
    technology::Id m_technology;
    /// File name
    std::string m_fileName;
    /// Container name
    std::string m_containerName;
    /// Identifier/entry number
    int m_id;
  };
} // namespace form::detail::experimental
#endif // FORM_CORE_TOKEN_HPP
