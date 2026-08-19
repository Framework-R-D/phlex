// Copyright (C) 2025 ...

#include "token.hpp"

#include <utility>

using namespace form::detail::experimental;

/// Placement-only constructor; id is left unset
Token::Token(std::string fileName, std::string containerName, technology::Id technology) :
  m_technology(technology),
  m_fileName(std::move(fileName)),
  m_containerName(std::move(containerName)),
  m_id(0),
  m_hasId(false)
{
}

/// Fully-specified constructor; id is set
Token::Token(std::string fileName,
             std::string containerName,
             technology::Id technology,
             std::uint64_t id) :
  m_technology(technology),
  m_fileName(std::move(fileName)),
  m_containerName(std::move(containerName)),
  m_id(id),
  m_hasId(true)
{
}

/// Access file name
std::string const& Token::fileName() const { return m_fileName; }
/// Access container name
std::string const& Token::containerName() const { return m_containerName; }
/// Access technology type
form::technology::Id Token::technology() const { return m_technology; }
/// Access identifier/entry number (0-based row)
std::uint64_t Token::id() const { return m_id; }
/// Whether an id has been set on this token
bool Token::hasId() const { return m_hasId; }
