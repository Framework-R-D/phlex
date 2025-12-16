add_library(layer_generator layer_generator.cpp)
target_link_libraries(layer_generator PRIVATE phlex::core)

add_library(generate_layers MODULE generate_layers.cpp)
target_link_libraries(generate_layers PRIVATE phlex::module layer_generator)
