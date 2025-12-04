#ifndef TEST_LAYER_GENERATOR_HPP
#define TEST_LAYER_GENERATOR_HPP

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"

#include <string>

namespace phlex::experimental {
  struct layer_spec {
    std::string parent_layer_name;
    std::size_t total_per_parent_data_cell;
    std::size_t starting_value = 0;
  };

  class layer_generator {
  public:
    layer_generator();
    void add_layer(std::string layer_name, layer_spec lspec);

    void operator()(framework_driver& driver) const { execute(driver, data_cell_index::base_ptr()); }

  private:
    void execute(framework_driver& driver, data_cell_index_ptr index, bool recurse = true) const;

    std::map<std::string, layer_spec> layers_;
    std::vector<std::string> layer_paths_{"/job"};

    using reverse_map_t = std::map<std::string, std::vector<std::string>>;
    reverse_map_t parent_to_children_;
  };
}

#endif // TEST_LAYER_GENERATOR_HPP
