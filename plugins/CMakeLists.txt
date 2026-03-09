# Phlex provided core plugins

# plugin for running Python algorithms in phlex
add_subdirectory(python)

add_library(layer_generator layer_generator.cpp)
target_link_libraries(layer_generator PRIVATE phlex::core)

install(TARGETS layer_generator LIBRARY)

# Companion library that links against phlex_core_internal so that tests
# using layer_generator do not mix the visibility-hidden and all-visible
# variants of phlex_core in the same process.
add_library(layer_generator_internal layer_generator.cpp)
target_link_libraries(layer_generator_internal PRIVATE phlex_core_internal)

add_library(generate_layers MODULE generate_layers.cpp)
target_link_libraries(generate_layers PRIVATE phlex::module layer_generator)

install(TARGETS generate_layers LIBRARY DESTINATION lib)
