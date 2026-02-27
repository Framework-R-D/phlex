#include <utility>

#ifndef FORM_STORAGE_STORAGE_UTIL_HPP
#define FORM_STORAGE_STORAGE_UTIL_HPP

namespace form::detail::experimental
{
  // Hash function for std::pair
  struct pair_hash {
    template <typename T1, typename T2>
    std::size_t operator()(std::pair<T1, T2> const& p) const
    {
      std::hash<T1> h1;
      std::hash<T2> h2;
      return h1(p.first) ^ (h2(p.second) << 1);
    }
  };
}

#endif // FORM_STORAGE_STORAGE_UTIL_HPP
