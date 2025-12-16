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

    layer_generator(layer_generator const&) = delete;
    layer_generator& operator=(layer_generator const&) = delete;
    layer_generator(layer_generator&&) = default;
    layer_generator& operator=(layer_generator&&) = default;

    void add_layer(std::string layer_name, layer_spec lspec);

    void operator()(framework_driver& driver) { execute(driver, data_cell_index::base_ptr()); }

    std::size_t emitted_cells(std::string layer_path = {}) const;

  private:
    void execute(framework_driver& driver, data_cell_index_ptr index, bool recurse = true);
    std::string parent_path(std::string const& layer_name,
                            std::string const& parent_layer_spec) const;
    void maybe_rebase_layer_paths(std::string const& layer_name,
                                  std::string const& parent_full_path);

    std::map<std::string, layer_spec> layers_;
    std::map<std::string, std::size_t> emitted_cells_;
    std::vector<std::string> layer_paths_{"/job"};

    using reverse_map_t = std::map<std::string, std::vector<std::string>>;
    reverse_map_t parent_to_children_;
  };

  // N.B. The layer_generator object must outlive any whatever uses it.
  std::function<void(framework_driver&)> driver_for_test(layer_generator& generator)
  {
    return [&generator](framework_driver& driver) mutable { generator(driver); };
  }
}

#endif // TEST_LAYER_GENERATOR_HPP
