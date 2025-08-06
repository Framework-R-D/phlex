// Copyright (C) 2025 ...

#ifndef __TOKEN_H__
#define __TOKEN_H__

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
    Token(const std::string& fileName,
          const std::string& containerName,
          int technology,
          int id = -1);

    /// Access file name
    const std::string& fileName() const;
    /// Access container name
    const std::string& containerName() const;
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
