# Provides phlex_apply_optimizations(target), which applies safe,
# performance-oriented compiler flags to a Phlex shared library target.
# Also defines the PHLEX_HIDE_SYMBOLS and PHLEX_ENABLE_IPO options and
# ensures their defaults are mutually consistent.
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
# Options and their interaction with CMAKE_BUILD_TYPE
#
#   Both PHLEX_HIDE_SYMBOLS and PHLEX_ENABLE_IPO are defined here so that their
#   defaults can be derived together.  The effective defaults are:
#
#     CMAKE_BUILD_TYPE    PHLEX_ENABLE_IPO  PHLEX_HIDE_SYMBOLS
#     ─────────────────   ───────────────   ─────────────────
#     Release             ON                ON
#     RelWithDebInfo      ON                ON
#     Debug / sanitizer   OFF               OFF
#     not set             OFF               OFF
#
#   Automatic co-adjustment: PHLEX_HIDE_SYMBOLS is forced ON whenever
#   PHLEX_ENABLE_IPO is ON, even if the user passes -DPHLEX_HIDE_SYMBOLS=OFF
#   on the command line.  LTO achieves its full potential only in combination
#   with hidden symbols — without -fvisibility=hidden,
#   -fno-semantic-interposition cannot be applied and the LTO optimizer must
#   treat every exported symbol as potentially interposable.
#   The enforcement uses set(... FORCE) after both options are resolved from
#   the cache, so the corrected value is always reflected in CMakeCache.txt.
#
#   -fno-semantic-interposition and -fno-plt are compile-level flags applied
#   per-target at build time (not at configure time), so they automatically
#   reflect the final option values regardless of how they were set.
#
# PHLEX_ENABLE_IPO (default ON for Release/RelWithDebInfo, OFF otherwise)
#   When ON, enables interprocedural optimization (LTO) for Release and
#   RelWithDebInfo configurations.  LTO is safe with symbol hiding because
#   export attributes preserve the complete exported-symbol set.  External
#   plugins compiled without LTO link against the normal exported-symbol table
#   and are unaffected.
#
# PHLEX_HIDE_SYMBOLS (default ON when PHLEX_ENABLE_IPO=ON or Release/RelWithDebInfo)
#   When ON: hidden-by-default visibility; export macros mark the public API.
#   When OFF: all symbols visible; _internal targets become thin INTERFACE
#   aliases of their public counterparts.


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
# Interprocedural optimization (LTO) — defined first so its value can inform
# the PHLEX_HIDE_SYMBOLS default below.
# ---------------------------------------------------------------------------
cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)

if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
  set(_phlex_ipo_default ON)
else()
  set(_phlex_ipo_default OFF)
endif()

option(
  PHLEX_ENABLE_IPO
  [=[Enable interprocedural optimization (LTO) for Release and RelWithDebInfo
builds.  Defaults to ON when CMAKE_BUILD_TYPE is Release or RelWithDebInfo.
When enabled, PHLEX_HIDE_SYMBOLS defaults to ON as well, since LTO achieves
its full potential only with a bounded exported-symbol set.]=]
  "${_phlex_ipo_default}"
)

# ---------------------------------------------------------------------------
# Symbol hiding — derived after PHLEX_ENABLE_IPO so that requesting IPO
# automatically enables hiding by default (they work together for maximum
# effect).
# ---------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$" OR PHLEX_ENABLE_IPO)
  set(_phlex_hide_default ON)
else()
  set(_phlex_hide_default OFF)
endif()

option(
  PHLEX_HIDE_SYMBOLS
  [=[Hide non-exported symbols in shared libraries (ON = curated API with
hidden-by-default visibility; OFF = all symbols visible).  Defaults to ON for
Release/RelWithDebInfo builds and whenever PHLEX_ENABLE_IPO=ON.  When
PHLEX_ENABLE_IPO=ON this option is forced ON regardless of the value passed on
the command line, since LTO achieves its full potential only with a bounded
exported-symbol set.]=]
  "${_phlex_hide_default}"
)

# ---------------------------------------------------------------------------
# Enforce option consistency: PHLEX_ENABLE_IPO=ON requires PHLEX_HIDE_SYMBOLS=ON.
# option() only sets the default when the cache variable is not yet present, so
# an explicit -DPHLEX_HIDE_SYMBOLS=OFF on the command line bypasses the default
# logic above.  After both options are resolved from the cache we therefore
# check for the invalid/suboptimal combination and force-correct it.
# ---------------------------------------------------------------------------
if(PHLEX_ENABLE_IPO AND NOT PHLEX_HIDE_SYMBOLS)
  message(
    STATUS
    "Phlex: PHLEX_HIDE_SYMBOLS forced ON because PHLEX_ENABLE_IPO=ON "
    "(LTO achieves full potential only with a bounded exported-symbol set; "
    "-fno-semantic-interposition requires hidden-by-default visibility)."
  )
  set(PHLEX_HIDE_SYMBOLS
      ON
      CACHE BOOL
      [=[Hide non-exported symbols in shared libraries (ON = curated API with
hidden-by-default visibility; OFF = all symbols visible).  Forced ON because
PHLEX_ENABLE_IPO=ON.]=]
      FORCE
  )
endif()

# ---------------------------------------------------------------------------
# Activate LTO (if enabled and supported)
# ---------------------------------------------------------------------------
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
