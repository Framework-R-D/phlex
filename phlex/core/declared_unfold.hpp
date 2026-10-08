#ifndef PHLEX_CORE_DECLARED_UNFOLD_HPP
#define PHLEX_CORE_DECLARED_UNFOLD_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  class products;
  struct unfold_flush;

  class PHLEX_CORE_EXPORT generator {
  public:
    explicit generator(phlex::experimental::product_store_const_ptr const& parent,
                       gsl::not_null<phlex::experimental::algorithm_name const*> node_name,
                       gsl::not_null<phlex::experimental::identifier const*> stage,
                       std::string const& child_layer_name);

    std::size_t child_layer_hash() const { return child_layer_hash_; }
    std::size_t child_count() const { return child_count_; }
    phlex::experimental::product_store_const_ptr make_child(std::size_t i, products new_products);

  private:
    phlex::experimental::product_store_ptr parent_;
    // References declared_unfold data members, which outlive this short-lived object.
    gsl::not_null<phlex::experimental::algorithm_name const*> node_name_;
    gsl::not_null<phlex::experimental::identifier const*> stage_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string const& child_layer_name_;
    std::size_t child_layer_hash_;
    std::size_t child_count_ = 0;
  };

  class PHLEX_CORE_EXPORT declared_unfold : public products_consumer {
  public:
    declared_unfold(phlex::experimental::algorithm_name name,
                    std::vector<std::string> predicates,
                    product_selectors input_products,
                    tbb::flow::graph& graph,
                    std::string child_layer);
    ~declared_unfold() override;

    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual tbb::flow::sender<index_message>& output_index_port() = 0;
    virtual tbb::flow::sender<unfold_flush>& flush_sender() = 0;
    virtual phlex::experimental::product_specifications const& output() const = 0;

    std::string const& child_layer() const noexcept { return child_layer_; }

  private:
    std::string child_layer_;
  };

  using declared_unfold_ptr = std::unique_ptr<declared_unfold>;
  using declared_unfolds = simple_ptr_map<declared_unfold_ptr>;
}

#endif // PHLEX_CORE_DECLARED_UNFOLD_HPP
