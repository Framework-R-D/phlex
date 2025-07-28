// Copyright (C) 2025 ...

#include "placement.hpp"

using namespace form::detail::experimental;

/// Constructor with initialization
Placement::Placement(const std::string& fileName,
                     const std::string& containerName,
                     int technology) :
  m_technology(technology), m_fileName(fileName), m_containerName(containerName)
{
}

/// Access file name
const std::string& Placement::fileName() const { return m_fileName; }
/// Set file name
Placement& Placement::setFileName(const std::string& fileName)
{
  m_fileName = fileName;
  return *this;
}
/// Access container name
const std::string& Placement::containerName() const { return m_containerName; }
/// Set container name
Placement& Placement::setContainerName(const std::string& containerName)
{
  m_containerName = containerName;
  return *this;
}
/// Access technology type
int Placement::technology() const { return m_technology; }
/// Set technology type
Placement& Placement::setTechnology(int technology)
{
  m_technology = technology;
  return *this;
}
