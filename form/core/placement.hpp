// Copyright (C) 2025 ...

#ifndef __PLACEMENT_H__
#define __PLACEMENT_H__

#include <string>

/* @class Placement
 * @brief This class holds all the necessary information to guide the writing of an object in a physical file.
 */
namespace form::detail::experimental {

  class Placement {
  public:
    /// Default Constructor
    Placement() = default;

    /// Constructor with initialization
    Placement(const std::string& fileName, const std::string& containerName, int technology);

    /// Access file name
    const std::string& fileName() const;
    /// Set file name
    Placement& setFileName(const std::string& fileName);
    /// Access container name
    const std::string& containerName() const;
    /// Set container name
    Placement& setContainerName(const std::string& containerName);
    /// Access technology type
    int technology() const;
    /// Set technology type
    Placement& setTechnology(int technology);

  private:
    /// Technology identifier
    int m_technology;
    /// File name
    std::string m_fileName;
    /// Container name
    std::string m_containerName;
  };
} // namespace form::detail::experimental

#endif
