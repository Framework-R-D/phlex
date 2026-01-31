#ifndef PHLEX_MODEL_DATA_CELL_INDEX_HPP
#define PHLEX_MODEL_DATA_CELL_INDEX_HPP

#include "phlex/model/fwd.hpp"

#include <cstddef>
#include <initializer_list>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phlex {
  class data_cell_index : public std::enable_shared_from_this<data_cell_index> {
  public:
    static data_cell_index const& base();
    static data_cell_index_ptr base_ptr();

    using hash_type = std::size_t;
    data_cell_index_ptr make_child(std::size_t data_cell_number, std::string layer_name) const;
    std::string const& layer_name() const noexcept;
    std::string layer_path() const;
    std::size_t depth() const noexcept;
    data_cell_index_ptr parent(std::string_view layer_name) const;
    data_cell_index_ptr parent() const noexcept;
    bool has_parent() const noexcept;
    std::size_t number() const;
    std::size_t hash() const noexcept;
    std::size_t layer_hash() const noexcept;
    bool operator==(data_cell_index const& other) const;
    bool operator<(data_cell_index const& other) const;

    std::string to_string() const;
    std::string to_string_this_layer() const;

    friend std::ostream& operator<<(std::ostream& os, data_cell_index const& id);

  private:
    data_cell_index();
    explicit data_cell_index(data_cell_index_ptr parent, std::size_t i, std::string layer_name);
    data_cell_index_ptr parent_{nullptr};
    std::size_t number_{-1ull};
    std::string layer_name_;
    std::size_t layer_hash_;
    std::size_t depth_{};
    hash_type hash_{0};
  };

  std::ostream& operator<<(std::ostream& os, data_cell_index const& id);
}

namespace std {
  template <>
  struct hash<phlex::data_cell_index> {
    std::size_t operator()(phlex::data_cell_index const& id) const noexcept { return id.hash(); }
  };
}

#endif // PHLEX_MODEL_DATA_CELL_INDEX_HPP
