# CMake generated Testfile for 
# Source directory: /workspace/test/plugins
# Build directory: /workspace/build-debug/test/plugins
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[job:add]=] "/workspace/build-debug/_deps/cetmodules-src/libexec/cet_exec_test" "--wd" "/workspace/build-debug/test/plugins/job:add.d" "--remove-on-failure" "" "--required-files" "" "--datafiles" "" "--skip-return-code" "247" "/workspace/build-debug/bin/phlex" "-c" "/workspace/test/plugins/add.jsonnet")
set_tests_properties([=[job:add]=] PROPERTIES  ENVIRONMENT "SPDLOG_LEVEL=debug;PHLEX_PLUGIN_PATH=/workspace/build-debug" LABELS "DEFAULT;RELEASE" SKIP_RETURN_CODE "247" WORKING_DIRECTORY "/workspace/build-debug/test/plugins/job:add.d" _BACKTRACE_TRIPLES "/workspace/build-debug/_deps/cetmodules-src/Modules/CetTest.cmake;1186;add_test;/workspace/build-debug/_deps/cetmodules-src/Modules/CetTest.cmake;1143;_cet_add_test_detail;/workspace/build-debug/_deps/cetmodules-src/Modules/CetTest.cmake;704;_cet_add_test;/workspace/test/plugins/CMakeLists.txt;10;cet_test;/workspace/test/plugins/CMakeLists.txt;0;")
