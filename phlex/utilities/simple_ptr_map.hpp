#ifndef phlex_utilities_simple_map_hpp
#define phlex_utilities_simple_map_hpp

// ==================================================================================
// The type simple_ptr_map<Ptr> is nearly equivalent to the type:
//
//   std::map<std::string, Ptr>
//
// except that only four public member functions are available:
//
//   - try_emplace(string, Ptr)
//   - begin()
//   - end()
//   - get(std::string const& key)
//
// The 'get(...') function returns a bare pointer to the stored element if it exists;
// otherwise the null pointer is returned.
// ==================================================================================

#include <map>
#include <memory>
#include <string>

namespace phlex::experimental {
  template <typename T>
  class simple_ptr_map;

  // Support std::unique_ptr<T> only for now
  template <typename T>
  class simple_ptr_map<std::unique_ptr<T>> {
    using Ptr = std::unique_ptr<T>;
    std::map<std::string, Ptr> data_;

  public:
    auto try_emplace(std::string node_name, Ptr ptr)
    {
      return data_.try_emplace(std::move(node_name), std::move(ptr));
    }

    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

    typename Ptr::element_type* get(std::string const& node_name) const
    {
      if (auto it = data_.find(node_name); it != data_.end()) {
        return it->second.get();
      }
      return nullptr;
    }
  };
}

#endif // phlex_utilities_simple_map_hpp
