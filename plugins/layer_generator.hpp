#ifndef PLUGINS_LAYER_GENERATOR_HPP
#define PLUGINS_LAYER_GENERATOR_HPP

// ==============================================================================================
// The layer_generator class enables the creation of a tree of layers, such as
//
//   job
//    │
//    ├ spill: 16
//    │  │
//    │  └ CRU: 4096
//    │
//    └ run: 16
//       │
//       └ APA: 2500
//
// To create the above tree of layers, the following function calls could be made:
//
//   layer_generator gen;
//   gen.add_layer("spill", {"job", 16});     // 16 spill data cells with job as parent
//   gen.add_layer("CRU",  {"spill", 256});   // 256 CRU data cells per spill parent
//   gen.add_layer("run", {"job", 16});       // 16 run data cells with job as parent
//   gen.add_layer("APA",  {"run", 150, 1});  // 150 APA data cells per run parent
//                                            // with first APA data cell number starting at 1
//
// ----------------------------------------------------------------------------------------------
// N.B. The layer generator can create data-layer hierarchies that are trees, and not
//      more general DAGs, where a data layer may have more than one parent.
// ==============================================================================================

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

#endif // PLUGINS_LAYER_GENERATOR_HPP
