# Provides phlex_apply_optimizations(target), which applies safe,
# performance-oriented compiler flags to a Phlex shared library target.
# Also defines the PHLEX_HIDE_SYMBOLS and PHLEX_ENABLE_IPO options and
# documents how they interact.
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
#   Both options can be overridden independently on the command line:
#
#     -DPHLEX_HIDE_SYMBOLS=ON  -DPHLEX_ENABLE_IPO=ON  → LTO + -fno-semantic-interposition
#                                                         (maximum optimization)
#     -DPHLEX_HIDE_SYMBOLS=OFF -DPHLEX_ENABLE_IPO=ON  → LTO only; -fno-semantic-interposition
#                                                         is NOT applied (valid, useful for
#                                                         benchmarking against the ON case)
#     -DPHLEX_HIDE_SYMBOLS=ON  -DPHLEX_ENABLE_IPO=OFF → -fno-semantic-interposition only
#     -DPHLEX_HIDE_SYMBOLS=OFF -DPHLEX_ENABLE_IPO=OFF → no special optimization flags
#
#   The per-target flags in phlex_apply_optimizations() self-adjust automatically
#   to reflect whichever combination is in effect.
#
# PHLEX_ENABLE_IPO (default ON for Release/RelWithDebInfo, OFF otherwise)
#   When ON, enables interprocedural optimization (LTO) for Release and
#   RelWithDebInfo configurations.  LTO is safe with or without symbol hiding
#   because export attributes preserve the complete exported-symbol set.
#   External plugins compiled without LTO link against the normal
#   exported-symbol table and are unaffected.
#
# PHLEX_HIDE_SYMBOLS (default ON for Release/RelWithDebInfo, OFF otherwise)
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
Can be combined with PHLEX_HIDE_SYMBOLS=ON for maximum optimization, or with
PHLEX_HIDE_SYMBOLS=OFF to benchmark LTO benefit without symbol hiding.]=]
  "${_phlex_ipo_default}"
)

# ---------------------------------------------------------------------------
# Symbol hiding — default follows CMAKE_BUILD_TYPE independently of IPO.
# The two options are orthogonal: both ON/OFF combinations are valid and
# produce different optimization profiles for benchmarking.
# ---------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
  set(_phlex_hide_default ON)
else()
  set(_phlex_hide_default OFF)
endif()

option(
  PHLEX_HIDE_SYMBOLS
  [=[Hide non-exported symbols in shared libraries (ON = curated API with
hidden-by-default visibility; OFF = all symbols visible).  Defaults to ON for
Release/RelWithDebInfo builds.
Combined with PHLEX_ENABLE_IPO=ON: -fno-semantic-interposition is also applied
for maximum optimization.  Setting OFF while PHLEX_ENABLE_IPO=ON is valid and
useful for comparing LTO performance with and without symbol hiding.]=]
  "${_phlex_hide_default}"
)

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
