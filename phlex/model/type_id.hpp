#ifndef PHLEX_MODE_TYPE_ID_HPP
#define PHLEX_MODE_TYPE_ID_HPP

#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/handle.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include <boost/core/demangle.hpp>
#include <boost/hash2/hash_append_fwd.hpp>
#include <boost/pfr/core.hpp>
#include <boost/pfr/traits.hpp>

#include <string>
#include <type_traits>
#include <vector>

// This is a type_id class to store the "product concept"
// Using our own class means we can treat, for example, all "List"s the same
namespace phlex::experimental {
  class type_id {
  public:
    // Least significant nibble will store the fundamental type
    enum class builtin : unsigned char {
      void_v = 0, // For completeness
      bool_v = 1,
      char_v = 2,
      int_v = 3,
      short_v = 4,
      long_v = 5,
      long_long_v = 6,
      float_v = 7,
      double_v = 8,
      long_double_v = 9
    };

    constexpr bool valid() const { return not(id_ == 0xFF); }

    constexpr bool is_unsigned() const { return valid() && (id_ & 0x10); }

    constexpr bool is_list() const { return valid() && (id_ & 0x20); }

    constexpr bool has_children() const { return valid() && (id_ & 0x40); }

    constexpr builtin fundamental() const { return static_cast<builtin>(id_ & 0x0F); }

    template <class Provider, class Hash, class Flavor>
    friend constexpr void tag_invoke(boost::hash2::hash_append_tag const&,
                                     Provider const&,
                                     Hash& h,
                                     Flavor const& f,
                                     type_id const* v)
    {
      boost::hash2::hash_append(h, f, v->id_);
      if (v->has_children()) {
        boost::hash2::hash_append(h, f, v->children_);
      }
    }

    constexpr std::strong_ordering operator<=>(type_id const& rhs) const
    {
      // This ordering is arbitrary but defined
      std::strong_ordering cmp_ids = id_ <=> rhs.id_;
      if (cmp_ids == std::strong_ordering::equal) {
        if (!this->has_children()) {
          return std::strong_ordering::equal;
        }
        return children_ <=> rhs.children_;
      }
      return cmp_ids;
    }

    constexpr bool operator==(type_id const& rhs) const
    {
      return (*this <=> rhs) == std::strong_ordering::equal;
    };

    bool exact_compare(type_id const& rhs) const { return *exact_ == *(rhs.exact_); }

    std::string exact_name() const { return boost::core::demangle(exact_->name()); }

    template <typename T>
    friend constexpr type_id make_type_id();
    friend struct fmt::formatter<type_id>;

  private:
    unsigned char id_ = 0xFF;
    std::type_info const* exact_{}; // Lifetime of type_info is defined to last until end of program

    // This is used only if the product type is a struct
    std::vector<type_id> children_;
  };

  using type_ids = std::vector<type_id>;

  namespace detail {
    template <typename T>
    consteval unsigned char make_type_id_helper_integral()
    {
      // Special case plain `char` in case it's unsigned
      if constexpr (std::is_same_v<char, T>) {
        return static_cast<unsigned char>(type_id::builtin::char_v);
      }

      unsigned char id = 0;
      // The following are integral types so we also need to get their signedness
      if constexpr (std::is_unsigned_v<T>) {
        id = 0x10;
      } else {
        id = 0x0;
      }
      using SignedT = std::make_signed_t<T>;

      if constexpr (std::is_same_v<signed char, SignedT>) {
        // We're choosing to treat signed char and char identically
        return id | static_cast<unsigned char>(type_id::builtin::char_v);
      } else if constexpr (std::is_same_v<int, SignedT>) {
        // ints are generally either long or long long, depending on implementation
        // Treating them separately here to reduce confusion
        id |= static_cast<unsigned char>(type_id::builtin::int_v);
        return id;
      } else if constexpr (std::is_same_v<short, SignedT>) {
        id |= static_cast<unsigned char>(type_id::builtin::short_v);
        return id;
      } else if constexpr (std::is_same_v<long, SignedT>) {
        id |= static_cast<unsigned char>(type_id::builtin::long_v);
        return id;
      } else if constexpr (std::is_same_v<long long, SignedT>) {
        id |= static_cast<unsigned char>(type_id::builtin::long_long_v);
        return id;
      } else {
        // If we got here, something went wrong
        // This condition is always false, but makes the error message more useful
        static_assert(std::is_same_v<T, void>, "Taking type_id of an unsupported fundamental type");
      }
    }

    template <typename T>
    consteval unsigned char make_type_id_helper_fundamental()
    {
      if constexpr (std::is_same_v<void, T>) {
        return static_cast<unsigned char>(type_id::builtin::void_v);
      } else if constexpr (std::is_same_v<bool, T>) {
        return static_cast<unsigned char>(type_id::builtin::bool_v);
      } else if constexpr (std::is_same_v<float, T>) {
        return static_cast<unsigned char>(type_id::builtin::float_v);
      } else if constexpr (std::is_same_v<double, T>) {
        return static_cast<unsigned char>(type_id::builtin::double_v);
      } else if constexpr (std::is_same_v<long double, T>) {
        return static_cast<unsigned char>(type_id::builtin::long_double_v);
      } else {
        return make_type_id_helper_integral<T>();
      }
    }

    template <typename A>
      requires(std::is_aggregate_v<A>)
    class aggregate_to_plain_tuple {
    private:
      template <std::size_t... Is>
      static consteval auto get_tuple(std::index_sequence<Is...>) -> auto
      {
        // Atomics are why we can't just use boost::pfr::structure_to_tuple
        return std::tuple<
          remove_atomic_t<std::remove_cvref_t<boost::pfr::tuple_element_t<Is, A>>>...>{};
      }

    public:
      using type = decltype(get_tuple(std::make_index_sequence<boost::pfr::tuple_size_v<A>>()));
    };

    template <typename A>
    using aggregate_to_plain_tuple_t = aggregate_to_plain_tuple<A>::type;

    template <typename T>
    class is_handle : public std::false_type {};

    template <typename T>
    class is_handle<handle<T>> : public std::true_type {};
  }

  // Forward declaration
  template <typename T1, typename... Ts>
  type_ids make_type_ids();

  template <typename T>
  constexpr type_id make_type_id()
  {
    // First deal with handles
    if constexpr (detail::is_handle<T>::value) {
      return make_type_id<typename T::value_type>();
    }

    type_id result{};
    using basic = remove_atomic_t<std::remove_cvref_t<std::remove_pointer_t<T>>>;
    if constexpr (std::is_fundamental_v<basic>) {
      result.id_ = detail::make_type_id_helper_fundamental<basic>();
    }

    // builtin arrays
    else if constexpr (std::is_array_v<basic>) {
      result = make_type_id<std::remove_all_extents_t<basic>>();
      result.id_ |= 0x20;
    }

    // classes (both containers and "simple" aggregates)
    else if constexpr (std::is_class_v<basic>) {
      if constexpr (contiguous_container<basic>) {
        result = make_type_id<typename basic::value_type>();
        result.id_ |= 0x20;
      } else if constexpr (std::is_aggregate_v<basic>) {
        // This case isn't evaluable at compile time because vector uses operator new
        using child_tuple = detail::aggregate_to_plain_tuple_t<basic>;
        result.id_ = 0x40; // has_children
        result.children_ = make_type_ids<child_tuple>();
      } else {
        // // If we got here, something went wrong
        // // This condition is always false, but makes the error message more useful
        // static_assert(contiguous_container<basic> || std::is_aggregate_v<basic>,
        //               "Taking type_id of an unsupported class type");
        // FIXME
        result.id_ = 0xFF;
      }
    }

    else {
      // If we got here, something went wrong
      // This condition is always false, but makes the error message more useful
      static_assert(std::is_fundamental_v<basic> || std::is_array_v<basic> ||
                      std::is_class_v<basic>,
                    "Taking type_id of an unsupported type");
    }

    result.exact_ = &typeid(basic);
    return result;
  }

  namespace detail {
    template <typename T>
    class tuple_type_ids {
    public:
      static type_ids get() { return {make_type_id<T>()}; }
    };

    template <typename... Ts>
    class tuple_type_ids<std::tuple<Ts...>> {
    public:
      static type_ids get() { return {make_type_id<Ts>()...}; }
    };

    template <typename... Ts>
    class tuple_type_ids<std::pair<Ts...>> {
    public:
      static type_ids get() { return {make_type_id<Ts>()...}; }
    };
  }

  template <typename T1, typename... Ts>
  type_ids make_type_ids()
  {
    if constexpr (sizeof...(Ts) == 0) {
      return detail::tuple_type_ids<T1>::get();
    } else {
      return type_ids{make_type_id<T1>(), make_type_id<Ts>()...};
    }
  }

  template <typename F>
  type_ids make_output_type_ids()
  {
    return make_type_ids<return_type<F>>();
  }

}

template <>
struct fmt::formatter<phlex::experimental::type_id> : formatter<std::string> {
  auto format(phlex::experimental::type_id type, format_context& ctx) const
  {
    using namespace std::string_literals;
    using namespace phlex::experimental;
    if (!type.valid()) {
      return fmt::formatter<std::string>::format("INVALID / EMPTY"s, ctx);
    }
    if (type.has_children()) {
      std::string const out = fmt::format(
        "{}STRUCT {{{}}}", type.is_list() ? "LIST " : "", fmt::join(type.children_, ", "));
      return fmt::formatter<std::string>::format(out, ctx);
    }

    std::string fundamental = "void"s;
    switch (type.fundamental()) {
    case type_id::builtin::void_v:
      fundamental = "void"s;
      break;
    case type_id::builtin::bool_v:
      fundamental = "bool"s;
      break;
    case type_id::builtin::char_v:
      fundamental = "char"s;
      break;
    case type_id::builtin::int_v:
      fundamental = "int"s;
      break;

    case type_id::builtin::short_v:
      fundamental = "short"s;
      break;

    case type_id::builtin::long_v:
      fundamental = "long"s;
      break;

    case type_id::builtin::long_long_v:
      fundamental = "long long"s;
      break;

    case type_id::builtin::float_v:
      fundamental = "float"s;
      break;

    case type_id::builtin::double_v:
      fundamental = "double"s;
      break;

    case type_id::builtin::long_double_v:
      fundamental = "long double"s;
      break;
    }
    std::string const out = fmt::format("{}{}{}",
                                        type.is_list() ? "LIST "s : ""s,
                                        type.is_unsigned() ? "unsigned "s : ""s,
                                        fundamental);
    return fmt::formatter<std::string>::format(out, ctx);
  }
};
#endif // PHLEX_MODE_TYPE_ID_HPP
