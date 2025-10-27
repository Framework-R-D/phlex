// Copyright (C) 2025 ...

#include "token.hpp"
#include <string>
#include <utility>

using namespace form::detail::experimental;

/// Constructor with initialization
Token::Token(std::string  fileName,
             std::string  containerName,
             int technology,
             int id) :
  m_technology(technology), m_fileName(std::move(fileName)), m_containerName(std::move(containerName)), m_id(id)
{
}

/// Access file name
auto Token::fileName() const -> std::string const& { return m_fileName; }
/// Access container name
auto Token::containerName() const -> std::string const& { return m_containerName; }
/// Access technology type
auto Token::technology() const -> int { return m_technology; }
/// Set technology type
/// Access identifier/entry number
auto Token::id() const -> int { return m_id; }
