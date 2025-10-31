#ifndef PHLEX_CORE_NODE_CATALOG_HPP
#define PHLEX_CORE_NODE_CATALOG_HPP

#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "boost/pfr.hpp"

#include <string>
#include <vector>

namespace phlex::experimental {
  struct node_catalog {
    template <typename Ptr>
    auto registrar_for(std::vector<std::string>& errors)
    {
      return registrar{boost::pfr::get<simple_ptr_map<Ptr>>(*this), errors};
    }

    std::size_t execution_counts(std::string const& node_name) const;
    std::size_t product_counts(std::string const& node_name) const;

    simple_ptr_map<declared_predicate_ptr> predicates{};
    simple_ptr_map<declared_observer_ptr> observers{};
    simple_ptr_map<declared_output_ptr> outputs{};
    simple_ptr_map<declared_fold_ptr> folds{};
    simple_ptr_map<declared_unfold_ptr> unfolds{};
    simple_ptr_map<declared_transform_ptr> transforms{};
  };
}

#endif // PHLEX_CORE_NODE_CATALOG_HPP
