// Copyright (C) 2025 ...

#include "token.hpp"

using namespace form::detail::experimental;

/// Constructor with initialization
Token::Token(const std::string& fileName, const std::string& containerName, int technology) :
  m_technology(technology), m_fileName(fileName), m_containerName(containerName)
{
}

/// Access file name
const std::string& Token::fileName() const { return m_fileName; }
/// Set file name
Token& Token::setFileName(const std::string& fileName)
{
  m_fileName = fileName;
  return *this;
}
/// Access container name
const std::string& Token::containerName() const { return m_containerName; }
/// Set container name
Token& Token::setContainerName(const std::string& containerName)
{
  m_containerName = containerName;
  return *this;
}
/// Access technology type
int Token::technology() const { return m_technology; }
/// Set technology type
Token& Token::setTechnology(int technology)
{
  m_technology = technology;
  return *this;
}

/// Access identifier/entry number
int Token::id() const { return m_id; }
/// Set identyfier/entry number
Token& Token::setId(int id)
{
  m_id = id;
  return *this;
}
