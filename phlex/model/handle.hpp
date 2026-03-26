#ifndef PHLEX_MODEL_HANDLE_HPP
#define PHLEX_MODEL_HANDLE_HPP

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/product_specification.hpp"

#include <type_traits>
#include <utility>
#include <variant>

namespace phlex {
  namespace experimental::detail {
    template <typename T>
    struct handle_value_type_impl {
      using type = std::remove_const_t<T>;
    };

    template <typename T>
    struct handle_value_type_impl<T&> {
      static_assert(std::is_const_v<T>,
                    "If template argument to handle_for is a reference, it must be const.");
      using type = std::remove_const_t<T>;
    };

    template <typename T>
    struct handle_value_type_impl<T*> {
      static_assert(std::is_const_v<T>,
                    "If template argument to handle_for is a pointer, the pointee must be const.");
      using type = std::remove_const_t<T>;
    };

    // Users are allowed to specify handle<T> as a parameter type to their algorithm
    template <typename T>
    struct handle_value_type_impl<handle<T>> {
      using type = typename handle_value_type_impl<T>::type;
    };

    template <typename T>
    using handle_value_type = typename handle_value_type_impl<T>::type;
  }

  // ==============================================================================================
  template <typename T>
  class handle {
  public:
    static_assert(std::same_as<T, experimental::detail::handle_value_type<T>>,
                  "Cannot create a handle with a template argument that is const-qualified, a "
                  "reference type, or a pointer type.");
    using value_type = T;
    using const_reference = value_type const&;
    using const_pointer = value_type const*;

    struct algorithm_name_view {
      std::string_view plugin;
      std::string_view algorithm;
    };

    // The 'product' parameter is not 'const_reference' to avoid avoid implicit type conversions.
    explicit handle(std::same_as<T> auto const& product,
                    data_cell_index const& id,
                    experimental::product_specification const& key) :
      product_{&product},
      id_{&id},
      creator_plugin_{key.plugin()},
      creator_algorithm_{key.algorithm()},
      suffix_(key.suffix())
    {
    }

    // Handles cannot be invalid
    handle() = delete;

    // Copy operations
    handle(handle const&) noexcept = default;
    handle& operator=(handle const&) noexcept = default;

    // Move operations
    handle(handle&&) noexcept = default;
    handle& operator=(handle&&) noexcept = default;

    const_pointer operator->() const noexcept { return product_; }
    [[nodiscard]] const_reference operator*() const noexcept { return *operator->(); }
    operator const_reference() const noexcept { return operator*(); }
    operator const_pointer() const noexcept { return operator->(); }

    auto const& data_cell_index() const noexcept { return *id_; }

    // Product specification information
    algorithm_name_view creator() const noexcept
    {
      return {std::string_view(creator_plugin_), std::string_view(creator_algorithm_)};
    }
    std::string_view suffix() const noexcept { return std::string_view(suffix_); }
    std::string_view layer() const noexcept { return std::string_view(id_->layer_name()); }
    std::string layer_path() const { return id_->layer_path(); }

    template <typename U>
    friend class handle;

    bool operator==(handle other) const noexcept
    {
      return product_ == other.product_ and id_ == other.id_;
    }

  private:
    const_pointer product_;           // Non-null, by construction
    class data_cell_index const* id_; // Non-null, by construction
    experimental::identifier creator_plugin_;
    experimental::identifier creator_algorithm_;
    experimental::identifier suffix_;
    experimental::type_id type_;
  };

  template <typename T>
  handle(T const&, data_cell_index const&, experimental::product_specification const&) -> handle<T>;
}

#endif // PHLEX_MODEL_HANDLE_HPP
