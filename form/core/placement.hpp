// Copyright (C) 2025 ...

#ifndef __PLACEMENT_H__
#define __PLACEMENT_H__

#include <string>

/* @class Placement
 * @brief This class holds all the necessary information to guide the writing of an object in a physical file.
 */
namespace form {
  namespace detail {
    namespace experimental {

      class Placement {
      public:
        /// Default Constructor
        Placement() = default;

        /// Constructor with initialization
        Placement(const std::string& fileName, const std::string& containerName, int technology) :
          m_technology(technology), m_fileName(fileName), m_containerName(containerName)
        {
        }

        /// Access file name
        const std::string& fileName() const { return m_fileName; }
        /// Set file name
        Placement& setFileName(const std::string& fileName)
        {
          m_fileName = fileName;
          return *this;
        }
        /// Access container name
        const std::string& containerName() const { return m_containerName; }
        /// Set container name
        Placement& setContainerName(const std::string& containerName)
        {
          m_containerName = containerName;
          return *this;
        }
        /// Access technology type
        int technology() const { return m_technology; }
        /// Set technology type
        Placement& setTechnology(int technology)
        {
          m_technology = technology;
          return *this;
        }

        /*
       /// Retrieve the string representation of the placement.
       const std::string toString() const;
       /// Build from the string representation of a placement.
       Placement& fromString(const std::string& from);
	*/

      private:
        /// Technology identifier
        int m_technology;
        /// File name
        std::string m_fileName;
        /// Container name
        std::string m_containerName;
      };
    } // namespace experimental
  } // namespace detail
} // namespace form

#endif
