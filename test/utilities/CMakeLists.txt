cet_test(sized_tuple SOURCE sized_tuple_test.cpp LIBRARIES phlex::utilities_internal)

cet_test(sleep_for USE_CATCH2_MAIN SOURCE sleep_for_test.cpp LIBRARIES
         phlex::utilities_internal
)
cet_test(
  thread_counter
  USE_CATCH2_MAIN
  SOURCE
  thread_counter_test.cpp
  LIBRARIES
  phlex::utilities_internal
  TBB::tbb
)
