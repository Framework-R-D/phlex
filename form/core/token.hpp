// Copyright (C) 2025 ...

#ifndef __TOKEN_HPP__
#define __TOKEN_HPP__

#include <string>

/* @class Token
 * @brief This class holds all the necessary information for reading of an object from a physical file.
 */
namespace form::detail::experimental {
  class Token {
  public:
    /// Default Constructor
    Token() = default;

    /// Constructor with initialization
    Token(std::string  fileName,
          std::string  containerName,
          int technology,
          int id = -1);

    /// Access file name
    std::string const& fileName() const;
    /// Access container name
    std::string const& containerName() const;
    /// Access technology type
    int technology() const;

    /// Access identifier/entry number
    int id() const;

  private:
    /// Technology identifier
    int m_technology;
    /// File name
    std::string m_fileName;
    /// Container name
    std::string m_containerName;
    /// Identifier/entry number
    int m_id;
  };
} // namespace form::detail::experimental
#endif
