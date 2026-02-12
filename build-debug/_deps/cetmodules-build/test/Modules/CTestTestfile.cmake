# CMake generated Testfile for 
# Source directory: /workspace/build-debug/_deps/cetmodules-src/test/Modules
# Build directory: /workspace/build-debug/_deps/cetmodules-build/test/Modules
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[parse_version_string-cmake_t]=] "/opt/spack-environments/phlex-ci/.spack-env/view/bin/cmake" "-DCMAKE_MODULE_PATH:STRING=/workspace/build-debug/_deps/cetmodules-src/test/Modules/../../Modules" "-P/workspace/build-debug/_deps/cetmodules-src/test/Modules/parse_version_string_t.cmake")
set_tests_properties([=[parse_version_string-cmake_t]=] PROPERTIES  LABELS "DEFAULT;RELEASE" _BACKTRACE_TRIPLES "/workspace/build-debug/_deps/cetmodules-src/test/Modules/CMakeLists.txt;1;add_test;/workspace/build-debug/_deps/cetmodules-src/test/Modules/CMakeLists.txt;0;")
add_test([=[version_cmp-cmake_t]=] "/workspace/build-debug/_deps/cetmodules-src/test/Modules/version_cmp-cmake_t")
set_tests_properties([=[version_cmp-cmake_t]=] PROPERTIES  LABELS "DEFAULT;RELEASE" _BACKTRACE_TRIPLES "/workspace/build-debug/_deps/cetmodules-src/test/Modules/CMakeLists.txt;9;add_test;/workspace/build-debug/_deps/cetmodules-src/test/Modules/CMakeLists.txt;0;")
