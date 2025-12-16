add_library(
  giantdata SHARED waveforms.cpp waveform_generator_input.cpp
                   waveform_generator.cpp user_algorithms.cpp
  )
target_link_libraries(giantdata PRIVATE TBB::tbb)

add_executable(unfold_transform_fold unfold_transform_fold.cpp)
target_link_libraries(
  unfold_transform_fold PRIVATE giantdata phlex::core layer_generator
  )
