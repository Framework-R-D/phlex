// Copyright (C) 2025 ...

#include "placement.hpp"

#include <utility>

using namespace form::detail::experimental;

/// Constructor with initialization
Placement::Placement(std::string fileName, std::string containerName, technology::Id technology) :
  m_technology(technology),
  m_fileName(std::move(fileName)),
  m_containerName(std::move(containerName))
{
}

/// Access file name
std::string const& Placement::fileName() const { return m_fileName; }
/// Access container name
std::string const& Placement::containerName() const { return m_containerName; }
/// Access technology type
form::technology::Id Placement::technology() const { return m_technology; }
