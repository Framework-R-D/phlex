cet_test(sized_tuple SOURCE sized_tuple.cpp LIBRARIES phlex::utilities)

cet_test(sleep_for USE_CATCH2_MAIN SOURCE sleep_for.cpp LIBRARIES
         phlex::utilities
)
cet_test(
  thread_counter
  USE_CATCH2_MAIN
  SOURCE
  thread_counter.cpp
  LIBRARIES
  phlex::utilities
  TBB::tbb
)
