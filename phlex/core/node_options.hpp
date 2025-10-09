#ifndef phlex_core_node_options_hpp
#define phlex_core_node_options_hpp

// =======================================================================================
// The facilities provided here will become simpler whenever "deducing this" is available
// in C++23 (which will largely replace CRTP).
// =======================================================================================

#include "phlex/core/detail/maybe_predicates.hpp"

#include <optional>
#include <string>
#include <vector>

namespace phlex::experimental {
  class configuration;

  template <typename T>
  class node_options {
  public:
    T& when(std::vector<std::string> predicates)
    {
      if (!predicates_) {
        predicates_ = std::move(predicates);
      }
      return self();
    }

    T& when(std::convertible_to<std::string> auto&&... names)
    {
      return when({std::forward<decltype(names)>(names)...});
    }

  protected:
    explicit node_options(configuration const* config)
    {
      if (!config) {
        return;
      }
      predicates_ = detail::maybe_predicates(config);
    }

    std::vector<std::string> release_predicates()
    {
      return std::move(predicates_).value_or(std::vector<std::string>{});
    }

  private:
    auto& self() { return *static_cast<T*>(this); }
    std::optional<std::vector<std::string>> predicates_{};
  };
}

#endif // phlex_core_node_options_hpp
