#[================================================================[.rst:
X
-
#]================================================================]
cmake_policy(PUSH)
cmake_minimum_required(VERSION 3.20...4.1 FATAL_ERROR)

get_property(_cet_langs GLOBAL PROPERTY ENABLED_LANGUAGES)
foreach(_cet_lang IN LISTS _cet_langs)
  if(CMAKE_${_cet_lang}_COMPILER_ID MATCHES "^(GNU|(Apple)?Clang)$")
    set(CMAKE_${_cet_lang}_FLAGS_RELWITHDEBINFO_INIT
        "-g -O3 -fno-omit-frame-pointer -DNDEBUG"
        )
  endif()
endforeach()
unset(_cet_langs)
unset(_cet_lang)

cmake_policy(POP)
