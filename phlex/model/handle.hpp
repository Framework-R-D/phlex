#ifndef PHLEX_MODEL_HANDLE_HPP
#define PHLEX_MODEL_HANDLE_HPP

#include "phlex/model/level_id.hpp"
#include "phlex/model/products.hpp"

#include "boost/core/demangle.hpp"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace phlex::experimental {

  namespace detail {
    template <typename T>
    using handle_type = std::remove_cvref_t<T>;

    template <typename T, typename U>
    concept same_handle_type = std::same_as<handle_type<T>, handle_type<U>>;
  }

  template <typename T>
  class handle {
    using err_t = std::string;

  public:
    using value_type = detail::handle_type<T>;
    using const_reference = value_type const&;
    using const_pointer = value_type const*;

    handle() = default;

    template <typename U>
    explicit handle(product<U> const& prod, level_id const& id = level_id::base())
      requires detail::same_handle_type<T, U>
      : rep_{&prod.obj}, id_{&id}
    {
    }

    explicit handle(std::variant<const_pointer, err_t> maybe_product, level_id const& id) :
      rep_{std::move(maybe_product)}, id_{&id}
    {
    }

    explicit handle(std::string err_msg, level_id const& id) : rep_{std::move(err_msg)}, id_{&id} {}

    const_pointer operator->() const
    {
      if (auto const* err = get_if<err_t>(&rep_)) {
        throw std::runtime_error(*err);
      }
      return get<const_pointer>(rep_);
    }
    [[nodiscard]] const_reference operator*() const { return *operator->(); }
    explicit operator bool() const noexcept { return get_if<const_pointer>(&rep_) != nullptr; }
    operator const_reference() const noexcept { return operator*(); }
    operator const_pointer() const noexcept { return operator->(); }

    auto const& level_id() const noexcept { return *id_; }

    template <typename U>
    friend class handle;

    template <typename U>
    bool operator==(handle<U> rhs) const noexcept
      requires detail::same_handle_type<T, U>
    {
      return rep_ == rhs.rep_;
    }

  private:
    std::variant<const_pointer, err_t> rep_{"Cannot dereference empty handle of type '" +
                                            boost::core::demangle(typeid(T).name()) + "'."};
    class level_id const* id_;
  };

  template <typename T>
  handle(product<T> const&) -> handle<T>;

  template <typename T>
  struct handle_ {
    using type = handle<std::remove_const_t<T>>;
  };

  template <typename T>
  struct handle_<T&> {
    static_assert(std::is_const_v<T>,
                  "If template argument to handle_for is a reference, it must be const.");
    using type = handle<std::remove_const_t<T>>;
  };

  template <typename T>
  struct handle_<T*> {
    static_assert(std::is_const_v<T>,
                  "If template argument to handle_for is a pointer, the pointee must be const.");
    using type = handle<std::remove_const_t<T>>;
  };

  template <typename T>
  struct handle_<handle<T>> {
    using type = handle<T>;
  };

  template <typename T>
  using handle_for = typename handle_<T>::type;
}

#endif // PHLEX_MODEL_HANDLE_HPP
