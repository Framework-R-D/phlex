# CMake generated Testfile for 
# Source directory: /workspace/test/demo-giantdata
# Build directory: /workspace/build-tsan/test/demo-giantdata
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[unfold_transform_fold]=] "/workspace/build-tsan/_deps/cetmodules-src/libexec/cet_exec_test" "--wd" "/workspace/build-tsan/test/demo-giantdata/unfold_transform_fold.d" "--remove-on-failure" "" "--required-files" "" "--datafiles" "" "--skip-return-code" "247" "/workspace/build-tsan/bin/unfold_transform_fold")
set_tests_properties([=[unfold_transform_fold]=] PROPERTIES  ENVIRONMENT "SPDLOG_LEVEL=debug" LABELS "DEFAULT;RELEASE" SKIP_RETURN_CODE "247" WORKING_DIRECTORY "/workspace/build-tsan/test/demo-giantdata/unfold_transform_fold.d" _BACKTRACE_TRIPLES "/workspace/build-tsan/_deps/cetmodules-src/Modules/CetTest.cmake;1186;add_test;/workspace/build-tsan/_deps/cetmodules-src/Modules/CetTest.cmake;1143;_cet_add_test_detail;/workspace/build-tsan/_deps/cetmodules-src/Modules/CetTest.cmake;704;_cet_add_test;/workspace/test/demo-giantdata/CMakeLists.txt;11;cet_test;/workspace/test/demo-giantdata/CMakeLists.txt;0;")
