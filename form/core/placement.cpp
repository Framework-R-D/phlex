// Copyright (C) 2025 ...

#include "placement.hpp"
#include <string>
#include <utility>

using namespace form::detail::experimental;

/// Constructor with initialization
Placement::Placement(std::string  fileName,
                     std::string  containerName,
                     int technology) :
  m_technology(technology), m_fileName(std::move(fileName)), m_containerName(std::move(containerName))
{
}

/// Access file name
auto Placement::fileName() const -> std::string const& { return m_fileName; }
/// Access container name
auto Placement::containerName() const -> std::string const& { return m_containerName; }
/// Access technology type
auto Placement::technology() const -> int { return m_technology; }
