#ifndef PHLEX_MODEL_HANDLE_HPP
#define PHLEX_MODEL_HANDLE_HPP

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/product_specification.hpp"

#include <gsl/pointers>

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace phlex {
  namespace detail::internal {
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
      using type = handle_value_type_impl<T>::type;
    };

    template <typename T>
    using handle_value_type = handle_value_type_impl<T>::type;
  }

  // ==============================================================================================
  template <typename T>
  class handle {
  public:
    static_assert(std::same_as<T, detail::internal::handle_value_type<T>>,
                  "Cannot create a handle with a template argument that is const-qualified, a "
                  "reference type, or a pointer type.");
    using value_type = T;
    using const_reference = value_type const&;
    using const_pointer = value_type const*;

    struct algorithm_name_view {
      std::string_view plugin;
      std::string_view algorithm;
    };

    explicit handle(gsl::not_null<const_pointer> const product,
                    gsl::not_null<data_cell_index const*> const index,
                    gsl::not_null<experimental::product_specification const*> const spec,
                    gsl::not_null<experimental::identifier const*> const stage) :
      product_{product}, index_{index}, stage_{stage}, spec_{spec}
    {
    }

    // Handles cannot be invalid
    handle() = delete;
    ~handle() = default;

    // Copy operations
    handle(handle const&) noexcept = default;
    handle& operator=(handle const&) noexcept = default;

    // Move operations
    handle(handle&&) noexcept = default;
    handle& operator=(handle&&) noexcept = default;

    const_pointer operator->() const noexcept { return product_; }
    [[nodiscard]] const_reference operator*() const noexcept { return *operator->(); }
    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    operator const_reference() const noexcept { return operator*(); }
    operator const_pointer() const noexcept { return operator->(); }
    // NOLINTEND(google-explicit-constructor)
    auto const& data_cell_index() const noexcept { return *index_; }

    // Product specification information
    algorithm_name_view creator() const noexcept
    {
      return {std::string_view(spec_->plugin()), std::string_view(spec_->algorithm())};
    }
    std::string_view suffix() const noexcept { return std::string_view(spec_->suffix()); }
    std::string_view layer() const noexcept { return std::string_view(index_->layer_name()); }
    std::string_view stage() const noexcept { return std::string_view(*stage_); }
    std::string layer_path() const { return index_->layer_path().to_string(); }

    template <typename U>
    friend class handle;

    bool operator==(handle other) const noexcept
    {
      return product_ == other.product_ and index_ == other.index_;
    }

  private:
    gsl::not_null<const_pointer> product_;
    gsl::not_null<class data_cell_index const*> index_;
    gsl::not_null<experimental::identifier const*> stage_;
    gsl::not_null<experimental::product_specification const*> spec_;
  };

  // Deduction guide
  template <typename T>
  handle(gsl::not_null<T const*>,
         gsl::not_null<data_cell_index const*>,
         gsl::not_null<experimental::product_specification const*>,
         gsl::not_null<experimental::identifier const*>) -> handle<T>;
}

#endif // PHLEX_MODEL_HANDLE_HPP
