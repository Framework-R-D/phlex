#ifndef PHLEX_MODE_TYPE_ID_HPP
#define PHLEX_MODE_TYPE_ID_HPP

#include <iterator>
#include <type_traits>

// This is a type_id class to store the "product concept"
// Using our own class means we can treat, for example, all "List"s the same
namespace phlex::experimental {
  class type_id {
  public:
    enum class builtin : unsigned char {
      void_v = 0, // For completeness
      bool_v = 1,
      char_v = 2,
      int_v = 3,
      short_v = 4,
      long_v = 5,
      long_long_v = 6,
      float_v = 7,
      double_v = 8
    };

    consteval bool is_unsigned() const { return id_ & 0x10; }

    consteval bool is_list() const { return id_ & 0x20; }

    consteval builtin fundamental() const { return static_cast<builtin>(id_ & 0x0F); }

    consteval bool operator==(type_id const& rhs) const { return id_ == rhs.id_; }
    consteval bool operator!=(type_id const& rhs) const = default;

    template <typename T>
    friend consteval type_id make_type_id();

  private:
    unsigned char id_ = 0xFF;
  };

  namespace details {
    template <typename T>
    consteval unsigned char make_type_id_helper_integral()
    {
      unsigned char id = 0;
      // The following are integral types so we also need to get their signedness
      if constexpr (std::is_unsigned_v<T>) {
        id = 0x10;
      } else {
        id = 0x0;
      }
      using T_signed = std::make_signed_t<T>;

      if constexpr (std::is_same_v<signed char, T_signed>) {
        // We're choosing to treat signed char and char identically
        id |= static_cast<unsigned char>(type_id::builtin::char_v);
        return id;
      } else if constexpr (std::is_same_v<int, T_signed>) {
        // ints are generally either long or long long, depending on implementation
        // Treating them separately here to reduce confusion
        id |= static_cast<unsigned char>(type_id::builtin::int_v);
        return id;
      } else if constexpr (std::is_same_v<short, T_signed>) {
        id |= static_cast<unsigned char>(type_id::builtin::short_v);
        return id;
      } else if constexpr (std::is_same_v<long, T_signed>) {
        id |= static_cast<unsigned char>(type_id::builtin::long_v);
        return id;
      } else if constexpr (std::is_same_v<long long, T_signed>) {
        id |= static_cast<unsigned char>(type_id::builtin::long_long_v);
        return id;
      } else {
        // If we got here, something went wrong
        static_assert(false, "Taking type_id of an unsupported type");
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
      } else {
        return make_type_id_helper_integral<T>();
      }
    }
  }

  template <typename T>
  consteval type_id make_type_id()
  {
    type_id result{};
    using basic = std::remove_cvref_t<T>;
    if constexpr (std::is_fundamental_v<basic>) {
      result.id_ = details::make_type_id_helper_fundamental<basic>();
      return result;
    }

    // arrays and vectors
    else if constexpr (std::is_array_v<basic>) {
      result = make_type_id<std::remove_all_extents_t<basic>>();
      result.id_ |= 0x20;
      return result;
    }

    else if constexpr (std::is_class_v<basic> &&
                       std::contiguous_iterator<typename basic::iterator>) {
      result = make_type_id<typename basic::value_type>();
      result.id_ |= 0x20;
      return result;
    } else {
      // If we got here, something went wrong
      static_assert(false, "Taking type_id of an unsupported type");
    }
  }
}

#endif // PHLEX_MODE_TYPE_ID_HPP
