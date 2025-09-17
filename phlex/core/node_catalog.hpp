#ifndef phlex_core_node_catalog_hpp
#define phlex_core_node_catalog_hpp

#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/registrar.hpp"

#include "boost/pfr.hpp"

#include <map>
#include <string>
#include <vector>

namespace phlex::experimental {
  template <typename Ptr>
  using declared_nodes = std::map<std::string, Ptr>;

  struct node_catalog {
    template <typename Ptr>
    auto registrar_for(std::vector<std::string>& errors)
    {
      return registrar{boost::pfr::get<declared_nodes<Ptr>>(*this), errors};
    }

    declared_nodes<declared_predicate_ptr> predicates_{};
    declared_nodes<declared_observer_ptr> observers_{};
    declared_nodes<declared_output_ptr> outputs_{};
    declared_nodes<declared_fold_ptr> folds_{};
    declared_nodes<declared_unfold_ptr> unfolds_{};
    declared_nodes<declared_transform_ptr> transforms_{};
  };
}

#endif // phlex_core_node_catalog_hpp
