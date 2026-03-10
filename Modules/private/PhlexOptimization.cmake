# Provides phlex_apply_optimizations(target), which applies safe,
# performance-oriented compiler flags to a Phlex shared library target.
#
# Two flags are applied (subject to compiler support and platform):
#
#   -fno-semantic-interposition (GCC >= 9, Clang >= 8)
#     The compiler may assume that exported symbols in this shared library are
#     not overridden at runtime by LD_PRELOAD or another DSO.  This is the
#     natural complement of -fvisibility=hidden: once the exported-symbol
#     surface is bounded by explicit export macros, treating those symbols as
#     non-interposable allows the compiler to inline, devirtualise, and
#     generate direct (non-PLT) calls for same-DSO accesses to exported
#     functions.
#
#     External plugins continue to call Phlex symbols through the standard
#     PLT/GOT mechanism; only code compiled as part of a Phlex shared library
#     itself benefits.
#
#     Applied only when PHLEX_HIDE_SYMBOLS=ON (export macros are present and
#     the exported-symbol set is well-defined).
#
#   -fno-plt (GCC >= 7.3, Clang >= 4, ELF platforms)
#     Calls FROM a Phlex library TO symbols in other shared libraries (TBB,
#     Boost, spdlog, ...) bypass the PLT stub and load the target address
#     directly from the GOT.  This replaces one level of indirection on every
#     cross-DSO call after first resolution.  Semantics are unchanged; only
#     the dispatch mechanism differs.
#
#     Not applied on Apple platforms: Mach-O uses two-level namespaces and the
#     PLT abstraction does not map directly to ELF semantics.
#
# Intentionally excluded:
#   -ffast-math / -funsafe-math-optimizations
#     Physics and numerical code relies on well-defined floating-point
#     semantics (NaN propagation, exact rounding, signed-zero behaviour).
#     These flags may silently produce incorrect numerical results and are
#     therefore not enabled.
#
# Interaction with CMAKE_BUILD_TYPE
#   -fno-semantic-interposition and -fno-plt are compile-level flags that are
#   applied unconditionally (subject to compiler / platform support) regardless
#   of build type; they complement but do not duplicate CMake's per-config
#   CMAKE_CXX_FLAGS_RELEASE (-O3 -DNDEBUG) and CMAKE_CXX_FLAGS_DEBUG (-g).
#
# PHLEX_ENABLE_IPO (default ON for Release/RelWithDebInfo, OFF otherwise)
#   When ON, enables interprocedural optimization (link-time optimization) for
#   Release and RelWithDebInfo configurations via the
#   CMAKE_INTERPROCEDURAL_OPTIMIZATION_* variables.
#
#   The default is determined from CMAKE_BUILD_TYPE at first configure time:
#     Release / RelWithDebInfo → ON   (maximise optimisation)
#     Debug / Coverage / sanitizers  → OFF (preserve debuggability)
#     not set (multi-config generators) → OFF (conservative fallback)
#
#   The option can always be overridden explicitly with -DPHLEX_ENABLE_IPO=ON|OFF.
#
#   LTO is safe with symbol hiding: export attributes preserve the complete
#   exported-symbol set; the linker cannot eliminate or rename any symbol that
#   carries a default-visibility attribute.  External plugins compiled without
#   LTO link against the normal exported-symbol table and are unaffected.

include_guard()

include(CheckCXXCompilerFlag)

# Probe flag availability once at module-load time (results are cached in the
# CMake cache and reused across reconfigures).
check_cxx_compiler_flag(
  "-fno-semantic-interposition"
  PHLEX_CXX_HAVE_NO_SEMANTIC_INTERPOSITION
)

if(NOT APPLE)
  check_cxx_compiler_flag("-fno-plt" PHLEX_CXX_HAVE_NO_PLT)
endif()

# ---------------------------------------------------------------------------
# Interprocedural optimization (LTO)
# ---------------------------------------------------------------------------
cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)

# Derive a sensible default: enable LTO automatically for Release-class builds
# so that cmake -DCMAKE_BUILD_TYPE=Release "does the right thing" without
# requiring a separate -DPHLEX_ENABLE_IPO=ON.  Debug/sanitizer/coverage builds
# keep the default OFF to preserve debuggability.
if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
  set(_phlex_ipo_default ON)
else()
  set(_phlex_ipo_default OFF)
endif()

option(
  PHLEX_ENABLE_IPO
  [=[Enable interprocedural optimization (LTO) for Release and RelWithDebInfo
builds.  Safe with symbol hiding because export macros preserve the complete
exported-symbol set.  External plugins compiled without LTO are unaffected.
Defaults to ON when CMAKE_BUILD_TYPE is Release or RelWithDebInfo.]=]
  "${_phlex_ipo_default}"
)

if(PHLEX_ENABLE_IPO)
  check_ipo_supported(
    RESULT _phlex_ipo_supported
    OUTPUT _phlex_ipo_output
    LANGUAGES CXX
  )
  if(_phlex_ipo_supported)
    # Set defaults for all targets created in this scope and below.  The
    # *_RELEASE and *_RELWITHDEBINFO variants leave Debug/Coverage/sanitizer
    # builds unaffected (those configs override optimization independently).
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    message(STATUS "Phlex: LTO enabled for Release and RelWithDebInfo builds")
  else()
    message(
      WARNING
      "Phlex: PHLEX_ENABLE_IPO=ON but LTO is not supported: ${_phlex_ipo_output}"
    )
  endif()
endif()

# ---------------------------------------------------------------------------
function(phlex_apply_optimizations target)
  # -fno-semantic-interposition pairs with PHLEX_HIDE_SYMBOLS: the compiler
  # may only treat exported symbols as non-interposable once the exported-
  # symbol surface has been explicitly bounded by export macros.
  if(PHLEX_HIDE_SYMBOLS AND PHLEX_CXX_HAVE_NO_SEMANTIC_INTERPOSITION)
    target_compile_options("${target}" PRIVATE "-fno-semantic-interposition")
  endif()

  # -fno-plt reduces cross-DSO call overhead on ELF (Linux) platforms.
  if(PHLEX_CXX_HAVE_NO_PLT)
    target_compile_options("${target}" PRIVATE "-fno-plt")
  endif()
endfunction()
