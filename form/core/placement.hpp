// Copyright (C) 2025 ...

#ifndef FORM_CORE_PLACEMENT_HPP
#define FORM_CORE_PLACEMENT_HPP

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
    Placement(std::string const& fileName, std::string const& containerName, int technology);

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

#endif // FORM_CORE_PLACEMENT_HPP
