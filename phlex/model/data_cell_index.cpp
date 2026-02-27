#include "phlex/model/data_cell_index.hpp"
#include "phlex/utilities/hashing.hpp"

#include "boost/algorithm/string.hpp"
#include "fmt/format.h"
#include "fmt/ranges.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>

using namespace std::string_literals;

namespace {

  std::vector<std::size_t> all_numbers(phlex::data_cell_index const& id)
  {
    if (!id.has_parent()) {
      return {};
    }

    auto const* current = &id;
    std::vector<std::size_t> result(id.depth());
    for (std::size_t i = id.depth(); i > 0; --i) {
      result[i - 1] = current->number();
      current = current->parent().get();
    }
    return result;
  }

}

namespace phlex {

  data_cell_index::data_cell_index() :
    layer_name_{"job"}, layer_hash_{phlex::experimental::hash(layer_name_)}
  {
  }

  data_cell_index::data_cell_index(data_cell_index_ptr parent,
                                   std::size_t i,
                                   std::string layer_name) :
    parent_{std::move(parent)},
    number_{i},
    layer_name_{std::move(layer_name)},
    layer_hash_{phlex::experimental::hash(parent_->layer_hash_, layer_name_)},
    depth_{parent_->depth_ + 1},
    hash_{phlex::experimental::hash(parent_->hash_, number_, layer_hash_)}
  {
    // FIXME: Should it be an error to create an ID with an empty name?
  }

  data_cell_index const& data_cell_index::base() { return *base_ptr(); }
  data_cell_index_ptr data_cell_index::base_ptr()
  {
    static data_cell_index_ptr base_id{new data_cell_index};
    return base_id;
  }

  std::string const& data_cell_index::layer_name() const noexcept { return layer_name_; }

  std::string data_cell_index::layer_path() const
  {
    std::vector layers_in_reverse{layer_name_};
    auto next_parent = parent();
    while (next_parent) {
      layers_in_reverse.push_back(next_parent->layer_name());
      next_parent = next_parent->parent();
    }
    return fmt::format("/{}", fmt::join(std::views::reverse(layers_in_reverse), "/"));
  }

  std::size_t data_cell_index::depth() const noexcept { return depth_; }

  data_cell_index_ptr data_cell_index::make_child(std::size_t const data_cell_number,
                                                  std::string child_layer_name) const
  {
    return data_cell_index_ptr{
      new data_cell_index{shared_from_this(), data_cell_number, std::move(child_layer_name)}};
  }

  bool data_cell_index::has_parent() const noexcept { return static_cast<bool>(parent_); }

  std::size_t data_cell_index::number() const { return number_; }
  std::size_t data_cell_index::hash() const noexcept { return hash_; }
  std::size_t data_cell_index::layer_hash() const noexcept { return layer_hash_; }

  bool data_cell_index::operator==(data_cell_index const& other) const
  {
    if (depth_ != other.depth_)
      return false;
    auto const same_numbers = number_ == other.number_;
    if (not parent_) {
      return same_numbers;
    }
    return *parent_ == *other.parent_ && same_numbers;
  }

  bool data_cell_index::operator<(data_cell_index const& other) const
  {
    auto these_numbers = all_numbers(*this);
    auto those_numbers = all_numbers(other);
    return std::lexicographical_compare(
      begin(these_numbers), end(these_numbers), begin(those_numbers), end(those_numbers));
  }

  data_cell_index_ptr data_cell_index::parent() const noexcept { return parent_; }

  data_cell_index_ptr data_cell_index::parent(std::string_view layer_name) const
  {
    data_cell_index_ptr parent = parent_;
    while (parent) {
      if (parent->layer_name_ == layer_name) {
        return parent;
      }
      parent = parent->parent_;
    }
    return nullptr;
  }

  std::string data_cell_index::to_string() const
  {
    // FIXME: prefix needs to be adjusted esp. if a root name can be supplied by the user.
    std::string prefix{"["}; //"root: ["};
    std::string result;
    std::string suffix{"]"};

    if (number_ != -1ull) {
      result = to_string_this_layer();
      auto parent = parent_;
      while (parent != nullptr and parent->number_ != -1ull) {
        result.insert(0, parent->to_string_this_layer() + ", ");
        parent = parent->parent_;
      }
    }
    return prefix + result + suffix;
  }

  std::string data_cell_index::to_string_this_layer() const
  {
    if (empty(layer_name_)) {
      return std::to_string(number_);
    }
    return layer_name_ + ":" + std::to_string(number_);
  }

  std::ostream& operator<<(std::ostream& os, data_cell_index const& id)
  {
    return os << id.to_string();
  }
}
