#ifndef PHLEX_CORE_CACHED_PRODUCT_STORES_HPP
#define PHLEX_CORE_CACHED_PRODUCT_STORES_HPP

// FIXME: only intended to be used in a single-threaded context as std::map is not
//        thread-safe.

#include "phlex/model/fwd.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"

namespace phlex::experimental {

  class cached_product_stores {
  public:
    product_store_ptr get_store(level_id_ptr id = level_id::base_ptr())
    {
      auto it = product_stores_.find(id->hash());
      if (it != cend(product_stores_)) {
        return it->second;
      }
      if (id == level_id::base_ptr()) {
        return new_store(product_store::base());
      }
      return new_store(
        get_store(id->parent())
          ->make_child(id->number(), id->level_name(), source_name_, stage::process));
    }

  private:
    product_store_ptr new_store(product_store_ptr const& store)
    {
      return product_stores_.try_emplace(store->id()->hash(), store).first->second;
    }

    std::string const source_name_{"Source"};
    std::map<level_id::hash_type, product_store_ptr> product_stores_{};
  };

}

#endif // PHLEX_CORE_CACHED_PRODUCT_STORES_HPP
