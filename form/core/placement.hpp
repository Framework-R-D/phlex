// Copyright (C) 2025 ...

#ifndef FORM_CORE_PLACEMENT_HPP
#define FORM_CORE_PLACEMENT_HPP

#include "core/technology.hpp"

#include <string>

/* @class placement
 * @brief This class holds all the necessary information to guide the writing of an object in a physical file.
 */
namespace form::detail::experimental {

  class placement {
  public:
    /// Default Constructor
    placement() = default;

    /// Constructor with initialization
    placement(std::string file_name, std::string container_name, technology::id technology);

    /// Access file name
    std::string const& file_name() const;
    /// Access container name
    std::string const& container_name() const;
    /// Access technology type
    technology::id technology() const;

  private:
    /// Technology identifier
    technology::id technology_{};
    /// File name
    std::string file_name_;
    /// Container name
    std::string container_name_;
  };
} // namespace form::detail::experimental

#endif // FORM_CORE_PLACEMENT_HPP
