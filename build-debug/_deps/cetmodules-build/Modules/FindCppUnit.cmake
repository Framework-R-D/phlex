#[================================================================[.rst:
FindCppUnit
-----------
#]================================================================]

include(CetFindPkgConfigPackage)
include(FindPackageHandleStandardArgs)

set(_cet_findcppunit_required ${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED})
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED)
set(_cet_findcppunit_quietly ${${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY})
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY TRUE)
cet_find_pkg_config_package(cppunit)
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY ${_cet_findcppunit_quietly})
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED ${_cet_findcppunit_required})
unset(_cet_findcppunit_required)
find_package_handle_standard_args(
  ${CMAKE_FIND_PACKAGE_NAME}
  VERSION_VAR ${CMAKE_FIND_PACKAGE_NAME}_VERSION
  REQUIRED_VARS CPPUNIT ${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIR
                ${CMAKE_FIND_PACKAGE_NAME}_LIBRARY
  )
