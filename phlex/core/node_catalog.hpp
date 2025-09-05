#ifndef phlex_core_node_catalog_hpp
#define phlex_core_node_catalog_hpp

#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/registrar.hpp"

#include <string>
#include <vector>

namespace phlex::experimental {
  struct node_catalog {
    auto register_output(std::vector<std::string>& errors) { return registrar{outputs_, errors}; }

    template <typename T>
    auto registrar_for(std::vector<std::string>& errors) = delete;

    declared_predicates predicates_{};
    declared_observers observers_{};
    declared_outputs outputs_{};
    declared_folds folds_{};
    declared_unfolds unfolds_{};
    declared_transforms transforms_{};
  };

  template <>
  inline auto node_catalog::registrar_for<declared_predicate_ptr>(std::vector<std::string>& errors)
  {
    return registrar{predicates_, errors};
  }

  template <>
  inline auto node_catalog::registrar_for<declared_observer_ptr>(std::vector<std::string>& errors)
  {
    return registrar{observers_, errors};
  }

  template <>
  inline auto node_catalog::registrar_for<declared_transform_ptr>(std::vector<std::string>& errors)
  {
    return registrar{transforms_, errors};
  }

  template <>
  inline auto node_catalog::registrar_for<declared_fold_ptr>(std::vector<std::string>& errors)
  {
    return registrar{folds_, errors};
  }

  template <>
  inline auto node_catalog::registrar_for<declared_unfold_ptr>(std::vector<std::string>& errors)
  {
    return registrar{unfolds_, errors};
  }
}

#endif // phlex_core_node_catalog_hpp
