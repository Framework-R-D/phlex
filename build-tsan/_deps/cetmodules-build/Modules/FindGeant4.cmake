#[============================================================[.rst:
FindGeant4
----------
#]============================================================]

set(_fg4_deps XercesC)
set(_fg4_libs
    G4FR
    G4GMocren
    G4OpenGL
    G4RayTracer
    G4Tree
    G4VRML
    G4analysis
    G4digits_hits
    G4error_propagation
    G4event
    G4geometry
    G4gl2ps
    G4global
    G4graphics_reps
    G4intercoms
    G4interfaces
    G4materials
    G4modeling
    G4parmodels
    G4particles
    G4persistency
    G4physicslists
    G4processes
    G4readout
    G4run
    G4track
    G4tracking
    G4visHepRep
    G4visXXX
    G4vis_management
    G4zlib
    )

unset(_fg4_fphsa_extra_args)
if(NOT Geant4_FOUND)
  find_package(Geant4 CONFIG)
  if(Geant4_FOUND)
    set(_fg4_need_tweaks TRUE)
  endif()
endif()

list(TRANSFORM _fg4_deps APPEND _FOUND OUTPUT_VARIABLE
                                       _fg4_fphsa_extra_required_vars
     )

if(Geant4_FOUND)
  unset(_fg4_missing_deps)
  foreach(_fg4_dep IN LISTS _fg4_deps)
    get_property(
      _fg4_${_fg4_dep}_alreadyTransitive GLOBAL
      PROPERTY _CMAKE_${_fg4_dep}_TRANSITIVE_DEPENDENCY
      )
    find_package(${_fg4_dep} QUIET)
    if(NOT DEFINED cet_${_fg4_dep}_alreadyTransitive
       OR cet_${_fg4_dep}_alreadyTransitive
       )
      set_property(
        GLOBAL PROPERTY _CMAKE_${_fg4_dep}_TRANSITIVE_DEPENDENCY TRUE
        )
    endif()
    unset(_fg4_${_fg4_dep}_alreadyTransitive)
    if(NOT ${_fg4_dep}_FOUND)
      list(APPEND _fg4_missing_deps ${_fg4_dep})
    endif()
  endforeach()
  unset(_fg4_dep)
  unset(_fg4_deps)
  if(NOT "${_fg4_missing_deps}" STREQUAL "")
    set(_fg4_fphsa_extra_args REASON_FAILURE_MESSAGE
                              "missing dependencies: ${_fg4_missing_deps}"
        )
    unset(_fg4_missing_deps)
  endif()
endif()

if(Geant4_FOUND AND _fg4_need_tweaks)
  foreach(_fg4_lib ${_fg4_libs})
    if(TARGET Geant4::${_fg4_lib})
      set_property(
        TARGET Geant4::${_fg4_lib}
        APPEND
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${Geant4_INCLUDE_DIR}/.."
        )
      if(_fg4_lib STREQUAL G4persistency)
        set_property(
          TARGET Geant4::${_fg4_lib}
          APPEND
          PROPERTY INTERFACE_LINK_LIBRARIES XercesC::XercesC
          )
      endif()
    endif()
  endforeach()
  unset(_fg4_lib)
  unset(_fg4_libs)
  unset(_fg4_need_tweaks)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  Geant4 CONFIG_MODE REQUIRED_VARS ${_fg4_fphsa_extra_required_vars}
                                   ${_fg4_fphsa_extra_args}
  )

unset(_fg4_fphsa_extra_required_vars)
unset(_fg4_fphsa_extra_args)
