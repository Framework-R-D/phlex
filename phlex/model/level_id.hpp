#ifndef phlex_model_level_id_hpp
#define phlex_model_level_id_hpp

#include "phlex/model/fwd.hpp"

#include <cstddef>
#include <initializer_list>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  class level_id : public std::enable_shared_from_this<level_id> {
  public:
    static level_id const& base();
    static level_id_ptr base_ptr();

    using hash_type = std::size_t;
    level_id_ptr make_child(std::size_t new_level_number, std::string level_name) const;
    std::string const& level_name() const noexcept;
    std::size_t depth() const noexcept;
    level_id_ptr parent(std::string const& level_name) const;
    level_id_ptr parent() const noexcept;
    bool has_parent() const noexcept;
    std::size_t number() const;
    std::size_t hash() const noexcept;
    std::size_t level_hash() const noexcept;
    bool operator==(level_id const& other) const;
    bool operator<(level_id const& other) const;

    std::string to_string() const;
    std::string to_string_this_level() const;

    friend std::ostream& operator<<(std::ostream& os, level_id const& id);

  private:
    level_id();
    explicit level_id(level_id_ptr parent, std::size_t i, std::string level_name);
    level_id_ptr parent_{nullptr};
    std::size_t number_{-1ull};
    std::string level_name_;
    std::size_t level_hash_;
    std::size_t depth_{};
    hash_type hash_{0};
  };

  level_id_ptr id_for(char const* str);
  level_id_ptr id_for(std::vector<std::size_t> nums);
  level_id_ptr operator""_id(char const* str, std::size_t);
  std::ostream& operator<<(std::ostream& os, level_id const& id);
}

namespace std {
  template <>
  struct hash<phlex::experimental::level_id> {
    std::size_t operator()(phlex::experimental::level_id const& id) const noexcept
    {
      return id.hash();
    }
  };
}

#endif // phlex_model_level_id_hpp
