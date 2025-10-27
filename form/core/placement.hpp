// Copyright (C) 2025 ...

#ifndef __PLACEMENT_HPP__
#define __PLACEMENT_HPP__

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
    Placement(std::string  fileName, std::string  containerName, int technology);

    /// Access file name
    std::string const& fileName() const;
    /// Access container name
    std::string const& containerName() const;
    /// Access technology type
    int technology() const;

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
