// Copyright (C) 2025 ...

#ifndef __TOKEN_H__
#define __TOKEN_H__

#include <string>

/* @class Token
 * @brief This class holds all the necessary information for reading of an object from a physical file.
 */

class Token {
public:
  /// Default Constructor
  Token() = default;

  /// Constructor with initialization
  Token(const std::string& fileName, const std::string& containerName, int technology) :
    m_technology(technology), m_fileName(fileName), m_containerName(containerName)
  {
  }

  /// Access file name
  const std::string& fileName() const { return m_fileName; }
  /// Set file name
  Token& setFileName(const std::string& fileName)
  {
    m_fileName = fileName;
    return *this;
  }
  /// Access container name
  const std::string& containerName() const { return m_containerName; }
  /// Set container name
  Token& setContainerName(const std::string& containerName)
  {
    m_containerName = containerName;
    return *this;
  }
  /// Access technology type
  int technology() const { return m_technology; }
  /// Set technology type
  Token& setTechnology(int technology)
  {
    m_technology = technology;
    return *this;
  }

  /// Access identifier/entry number
  int id() const { return m_id; }
  /// Set identyfier/entry number
  Token& setId(int id)
  {
    m_id = id;
    return *this;
  }

  /*
	/// Retrieve the string representation of the placement.
	const std::string toString() const;
	/// Build from the string representation of a placement.
	Token& fromString(const std::string& from);
	*/

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

#endif
