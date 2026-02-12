# CMake generated Testfile for 
# Source directory: /workspace/test/memory-checks
# Build directory: /workspace/build-debug/test/memory-checks
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[many_events]=] "/workspace/build-debug/_deps/cetmodules-src/libexec/cet_exec_test" "--wd" "/workspace/build-debug/test/memory-checks/many_events.d" "--remove-on-failure" "" "--required-files" "" "--datafiles" "" "--skip-return-code" "247" "/workspace/build-debug/bin/many_events")
set_tests_properties([=[many_events]=] PROPERTIES  ENVIRONMENT "SPDLOG_LEVEL=debug" LABELS "DEFAULT;RELEASE" SKIP_RETURN_CODE "247" WORKING_DIRECTORY "/workspace/build-debug/test/memory-checks/many_events.d" _BACKTRACE_TRIPLES "/workspace/build-debug/_deps/cetmodules-src/Modules/CetTest.cmake;1186;add_test;/workspace/build-debug/_deps/cetmodules-src/Modules/CetTest.cmake;1143;_cet_add_test_detail;/workspace/build-debug/_deps/cetmodules-src/Modules/CetTest.cmake;704;_cet_add_test;/workspace/test/memory-checks/CMakeLists.txt;1;cet_test;/workspace/test/memory-checks/CMakeLists.txt;0;")
