#ifndef PHLEX_MODEL_HANDLE_HPP
#define PHLEX_MODEL_HANDLE_HPP

#include "phlex/model/level_id.hpp"

#include <type_traits>
#include <utility>
#include <variant>

namespace phlex::experimental {
  template <typename T>
  class handle;

  namespace detail {
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
    static_assert(std::same_as<T, detail::handle_value_type<T>>,
                  "Cannot create a handle with a template argument that is const-qualified, a "
                  "reference type, or a pointer type.");
    using value_type = T;
    using const_reference = value_type const&;
    using const_pointer = value_type const*;

    // The 'product' parameter is not 'const_reference' to avoid avoid implicit type conversions.
    explicit handle(std::same_as<T> auto const& product, level_id const& id) :
      product_{&product}, id_{&id}
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

    auto const& level_id() const noexcept { return *id_; }

    template <typename U>
    friend class handle;

    bool operator==(handle other) const noexcept
    {
      return product_ == other.product_ and id_ == other.id_;
    }

  private:
    const_pointer product_;    // Non-null, by construction
    class level_id const* id_; // Non-null, by construction
  };

  template <typename T>
  handle(T const&, level_id const&) -> handle<T>;
}

#endif // PHLEX_MODEL_HANDLE_HPP
