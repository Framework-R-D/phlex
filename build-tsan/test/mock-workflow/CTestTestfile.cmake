# CMake generated Testfile for 
# Source directory: /workspace/test/mock-workflow
# Build directory: /workspace/build-tsan/test/mock-workflow
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[mock-workflow]=] "/workspace/build-tsan/_deps/cetmodules-src/libexec/cet_exec_test" "--wd" "/workspace/build-tsan/test/mock-workflow/mock-workflow.d" "--remove-on-failure" "" "--required-files" "" "--datafiles" "" "--skip-return-code" "247" "/workspace/build-tsan/bin/phlex" "-c" "/workspace/test/mock-workflow/mock-workflow.jsonnet")
set_tests_properties([=[mock-workflow]=] PROPERTIES  ENVIRONMENT "SPDLOG_LEVEL=debug;PHLEX_PLUGIN_PATH=/workspace/build-tsan" LABELS "DEFAULT;RELEASE" SKIP_RETURN_CODE "247" WORKING_DIRECTORY "/workspace/build-tsan/test/mock-workflow/mock-workflow.d" _BACKTRACE_TRIPLES "/workspace/build-tsan/_deps/cetmodules-src/Modules/CetTest.cmake;1186;add_test;/workspace/build-tsan/_deps/cetmodules-src/Modules/CetTest.cmake;1143;_cet_add_test_detail;/workspace/build-tsan/_deps/cetmodules-src/Modules/CetTest.cmake;704;_cet_add_test;/workspace/test/mock-workflow/CMakeLists.txt;23;cet_test;/workspace/test/mock-workflow/CMakeLists.txt;0;")
