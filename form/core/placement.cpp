// Copyright (C) 2025 ...

#include "placement.hpp"

using namespace form::detail::experimental;

/// Constructor with initialization
Placement::Placement(std::string const& fileName,
                     std::string const& containerName,
                     int technology) :
  m_technology(technology), m_fileName(fileName), m_containerName(containerName)
{
}

/// Access file name
std::string const& Placement::fileName() const { return m_fileName; }
/// Access container name
std::string const& Placement::containerName() const { return m_containerName; }
/// Access technology type
int Placement::technology() const { return m_technology; }
