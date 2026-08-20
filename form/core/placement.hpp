// Copyright (C) 2025 ...

#ifndef FORM_CORE_PLACEMENT_HPP
#define FORM_CORE_PLACEMENT_HPP

#include "core/technology.hpp"

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
    Placement(std::string fileName, std::string containerName, technology::Id technology);

    /// Access file name
    std::string const& fileName() const;
    /// Access container name
    std::string const& containerName() const;
    /// Access technology type
    technology::Id technology() const;

  private:
    /// Technology identifier
    technology::Id m_technology{};
    /// File name
    std::string m_fileName;
    /// Container name
    std::string m_containerName;
  };
} // namespace form::detail::experimental

#endif // FORM_CORE_PLACEMENT_HPP
