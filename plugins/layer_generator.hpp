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
//   auto gen = layer_generator::make();
//   gen->add_layer("spill", {"job", 16});     // 16 spill data cells with job as parent
//   gen->add_layer("CRU",  {"spill", 256});   // 256 CRU data cells per spill parent
//   gen->add_layer("run", {"job", 16});       // 16 run data cells with job as parent
//   gen->add_layer("APA",  {"run", 150, 1});  // 150 APA data cells per run parent
//                                             // with first APA data cell number starting at 1
//
// ----------------------------------------------------------------------------------------------
// N.B. The layer generator can create data-layer hierarchies that are trees, and not
//      more general DAGs, where a data layer may have more than one parent.
// ==============================================================================================

#include "phlex/driver.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/index_generator.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phlex::experimental {
  struct layer_spec {
    std::string parent_layer_name;
    std::size_t total_per_parent_data_cell{};
    std::size_t starting_value = 0;
  };

  // Inherit enable_shared_from_this so driver_function can capture a shared_ptr to this,
  // ensuring layer_generator remains alive while the returned driver callable is in use.
  class layer_generator : public std::enable_shared_from_this<layer_generator> {
  public:
    [[nodiscard]] static std::shared_ptr<layer_generator> make();
    ~layer_generator() = default;

    layer_generator(layer_generator const&) = delete;
    layer_generator& operator=(layer_generator const&) = delete;
    layer_generator(layer_generator&&) = default;
    layer_generator& operator=(layer_generator&&) = default;

    void add_layer(std::string layer_name, layer_spec lspec);

    index_generator indices();
    std::function<void(data_cell_yielder const)> driver_function();

    fixed_hierarchy hierarchy() const;
    std::size_t emitted_cell_count(std::string layer_path = {}) const;

  private:
    layer_generator();

    index_generator execute(data_cell_index_ptr cell);
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
}

#endif // PLUGINS_LAYER_GENERATOR_HPP
