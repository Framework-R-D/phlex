// Copyright (C) 2025 ...

#include "token.hpp"

#include <utility>

using namespace form::detail::experimental;

/// Constructor with initialization
Token::Token(std::string fileName, std::string containerName, int technology, int id) :
  m_technology(technology),
  m_fileName(std::move(fileName)),
  m_containerName(std::move(containerName)),
  m_id(id)
{
}

/// Access file name
std::string const& Token::fileName() const { return m_fileName; }
/// Access container name
std::string const& Token::containerName() const { return m_containerName; }
/// Access technology type
int Token::technology() const { return m_technology; }
/// Set technology type
/// Access identifier/entry number
int Token::id() const { return m_id; }
