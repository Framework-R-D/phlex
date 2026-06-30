#ifndef PHLEX_CORE_NODE_CATALOG_HPP
#define PHLEX_CORE_NODE_CATALOG_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/producer_catalog.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/provider_node.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/source.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <string>
#include <type_traits>
#include <vector>

namespace phlex::detail {
  struct PHLEX_CORE_EXPORT node_catalog {
    template <typename Ptr>
    auto registrar_for(std::vector<std::string>& errors)
    {
      return registrar{ptr_map_for<Ptr>(), errors};
    }

    phlex::detail::source_vector sources_for(std::vector<std::string> const& keys) const;

    std::size_t execution_count(std::string const& node_name) const;
    std::vector<products_consumer*> consumers() const;
    producer_catalog producers() const;

    phlex::detail::simple_ptr_map<declared_predicate_ptr> predicates{};
    phlex::detail::simple_ptr_map<declared_observer_ptr> observers{};
    phlex::detail::simple_ptr_map<declared_output_ptr> outputs{};
    phlex::detail::simple_ptr_map<declared_fold_ptr> folds{};
    phlex::detail::simple_ptr_map<declared_unfold_ptr> unfolds{};
    phlex::detail::simple_ptr_map<declared_transform_ptr> transforms{};
    phlex::detail::simple_ptr_map<provider_node_ptr> providers{};
    phlex::detail::simple_ptr_map<phlex::detail::source_ptr> sources{};

  private:
    template <typename>
    static constexpr bool unknown_ptr_type_v{false};

    template <typename Ptr>
    auto& ptr_map_for()
    {
      // Note: We can eventually revert to boost::pfr::get once we move beyond LLVM 20.
      if constexpr (std::is_same_v<Ptr, declared_predicate_ptr>) {
        return predicates;
      } else if constexpr (std::is_same_v<Ptr, declared_observer_ptr>) {
        return observers;
      } else if constexpr (std::is_same_v<Ptr, declared_output_ptr>) {
        return outputs;
      } else if constexpr (std::is_same_v<Ptr, declared_fold_ptr>) {
        return folds;
      } else if constexpr (std::is_same_v<Ptr, declared_unfold_ptr>) {
        return unfolds;
      } else if constexpr (std::is_same_v<Ptr, declared_transform_ptr>) {
        return transforms;
      } else if constexpr (std::is_same_v<Ptr, provider_node_ptr>) {
        return providers;
      } else if constexpr (std::is_same_v<Ptr, phlex::detail::source_ptr>) {
        return sources;
      } else {
        static_assert(unknown_ptr_type_v<Ptr>, "Unsupported node pointer type");
      }
    }
  };
}

#endif // PHLEX_CORE_NODE_CATALOG_HPP
