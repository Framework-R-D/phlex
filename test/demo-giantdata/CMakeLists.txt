add_library(
  giantdata
  SHARED
  waveforms.cpp
  waveform_generator_input.cpp
  waveform_generator.cpp
  user_algorithms.cpp
)
target_link_libraries(giantdata PRIVATE TBB::tbb)

cet_test(
  unfold_transform_fold
  SOURCE
  unfold_transform_fold.cpp
  LIBRARIES
  giantdata
  phlex::core
  layer_generator
  USE_CATCH2_MAIN
)
