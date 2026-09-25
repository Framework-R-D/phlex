#include "phlex/model/product_store.hpp"

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/products.hpp"

#include <gsl/pointers>

#include <memory>
#include <utility>

namespace phlex::experimental {

  product_store::product_store(data_cell_index_ptr id,
                               gsl::not_null<algorithm_name const*> source,
                               gsl::not_null<identifier const*> stage,
                               phlex::detail::products new_products) :
    products_{std::move(new_products)}, id_{std::move(id)}, source_{source}, stage_{stage}
  {
  }

  product_store::~product_store() = default;

  product_store_ptr product_store::base(gsl::not_null<algorithm_name const*> creator,
                                        gsl::not_null<identifier const*> stage)
  {
    return std::make_shared<product_store>(data_cell_index::job(), creator, stage);
  }
  identifier const& product_store::layer_name() const noexcept { return id_->layer_name(); }
  identifier const& product_store::stage() const noexcept { return *stage_; }
  algorithm_name const& product_store::source() const noexcept { return *source_; }
  data_cell_index_ptr const& product_store::index() const noexcept { return id_; }

  product_store_ptr const& detail::more_derived(product_store_ptr const& a,
                                                product_store_ptr const& b)
  {
    if (a->index()->depth() > b->index()->depth()) {
      return a; // NOLINT(bugprone-return-const-ref-from-parameter)
    }
    return b; // NOLINT(bugprone-return-const-ref-from-parameter)
  }

  product_store_const_ptr const& detail::more_derived(product_store_const_ptr const& a,
                                                      product_store_const_ptr const& b)
  {
    if (a->index()->depth() > b->index()->depth()) {
      return a; // NOLINT(bugprone-return-const-ref-from-parameter)
    }
    return b; // NOLINT(bugprone-return-const-ref-from-parameter)
  }

  algorithm_name product_store::default_source()
  {
    static algorithm_name const def = algorithm_name::create("[Source]");
    return def;
  }
}
