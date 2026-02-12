#[================================================================[.rst:
X
-
#]================================================================]

if(NOT Eigen3_FOUND)
  find_package(Eigen3 CONFIG)
endif()

if(Eigen3_FOUND
   AND NOT TARGET Eigen3::Eigen
   AND EIGEN3_INCLUDE_DIRS
   )
  # < Eigen 3.4.0
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(
    Eigen3::Eigen
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIRS}"
               INTERFACE_COMPILE_DEFINITIONS "${EIGEN3_DEFINITIONS}"
    )
endif()
