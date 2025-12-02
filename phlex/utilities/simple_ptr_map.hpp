#ifndef PHLEX_UTILITIES_SIMPLE_PTR_MAP_HPP
#define PHLEX_UTILITIES_SIMPLE_PTR_MAP_HPP

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
//   - get(std::string const& key) [[not provided by std::map]]
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
    using ptr_t = std::unique_ptr<T>;
    std::map<std::string, ptr_t> data_;

  public:
    // std::map<std::string, ptr_t> has a default constructor that does
    // not invoke the deleted default constructor of its
    // std::pair<std::string const, ptr_t> data member, but the compiler
    // doesn't look at that when deciding whether to delete the default
    // constructor of simple_ptr_map
    simple_ptr_map() = default;
    ~simple_ptr_map() = default;

    // Explicitly disable copying
    simple_ptr_map(simple_ptr_map const&) = delete;
    simple_ptr_map& operator=(simple_ptr_map const&) = delete;

    // Enable moves
    simple_ptr_map(simple_ptr_map&&) = default;
    simple_ptr_map& operator=(simple_ptr_map&&) = default;

    auto try_emplace(std::string node_name, ptr_t ptr)
    {
      return data_.try_emplace(std::move(node_name), std::move(ptr));
    }

    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

    typename ptr_t::element_type* get(std::string const& node_name) const
    {
      if (auto it = data_.find(node_name); it != data_.end()) {
        return it->second.get();
      }
      return nullptr;
    }
  };
}

#endif // PHLEX_UTILITIES_SIMPLE_PTR_MAP_HPP
