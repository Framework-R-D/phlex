#ifndef PHLEX_CORE_RESOURCE_ENTRIES_HPP
#define PHLEX_CORE_RESOURCE_ENTRIES_HPP

#include "phlex/core/resource/concepts.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <gsl/pointers>

#include <memory>
#include <utility>

namespace phlex::detail {
  class resource_base {
  public:
    virtual ~resource_base() = default;
  };

  template <unlimited_resource T>
  class unlimited_resource_entry : public resource_base {
  public:
    using resource_type = T;

    template <typename... Args>
    explicit unlimited_resource_entry(Args&&... args) : resource_{std::forward<Args>(args)...}
    {
    }

    gsl::not_null<T const*> access() const noexcept { return std::addressof(resource_); }

  private:
    T resource_;
  };

  template <single_token_resource T>
  class single_token_resource_entry : public resource_base {
  public:
    using resource_type = T;
    using token_type = T::token_type;

    template <typename... Args>
    explicit single_token_resource_entry(Args&&... args) :
      resource_{std::forward<Args>(args)...}, limiter_{make_token(resource_)}
    {
    }

    tbb::flow::resource_limiter<token_type>& limiter() noexcept { return limiter_; }

  private:
    static token_type make_token(T& resource)
    {
      if constexpr (std::same_as<token_type, T>) {
        return resource;
      } else {
        return std::addressof(resource);
      }
    }

    // The framework owns the resource, while TBB uses the limiter to limit access to it.
    // Declare resource_ first so it outlives the limiter, which holds a pointer to it.
    T resource_;
    tbb::flow::resource_limiter<token_type> limiter_;
  };
}

#endif // PHLEX_CORE_RESOURCE_ENTRIES_HPP
