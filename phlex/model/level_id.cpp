#include "phlex/model/level_id.hpp"
#include "phlex/utilities/hashing.hpp"


#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cstddef>
#include <iterator>
#include <map>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace {

  auto all_numbers(phlex::experimental::level_id const& id) -> std::vector<std::size_t>
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

namespace phlex::experimental {

  level_id::level_id() : level_name_{"job"}, level_hash_{phlex::experimental::hash(level_name_)} {}

  level_id::level_id(level_id_ptr parent, std::size_t i, std::string level_name) :
    parent_{std::move(parent)},
    number_{i},
    level_name_{std::move(level_name)},
    level_hash_{phlex::experimental::hash(parent_->level_hash_, level_name_)},
    depth_{parent_->depth_ + 1},
    hash_{phlex::experimental::hash(parent_->hash_, number_, level_hash_)}
  {
    // FIXME: Should it be an error to create an ID with an empty name?
  }

  auto level_id::base() -> level_id const& { return *base_ptr(); }
  auto level_id::base_ptr() -> level_id_ptr
  {
    static phlex::experimental::level_id_ptr base_id{new level_id};
    return base_id;
  }

  auto level_id::level_name() const noexcept -> std::string const& { return level_name_; }
  auto level_id::depth() const noexcept -> std::size_t { return depth_; }

  auto level_id::make_child(std::size_t const new_level_number,
                                    std::string new_level_name) const -> level_id_ptr
  {
    return level_id_ptr{
      new level_id{shared_from_this(), new_level_number, std::move(new_level_name)}};
  }

  auto level_id::has_parent() const noexcept -> bool { return static_cast<bool>(parent_); }

  auto level_id::number() const -> std::size_t { return number_; }
  auto level_id::hash() const noexcept -> std::size_t { return hash_; }
  auto level_id::level_hash() const noexcept -> std::size_t { return level_hash_; }

  auto level_id::operator==(level_id const& other) const -> bool
  {
    if (depth_ != other.depth_)
      return false;
    auto const same_numbers = number_ == other.number_;
    if (not parent_) {
      return same_numbers;
    }
    return *parent_ == *other.parent_ && same_numbers;
  }

  auto level_id::operator<(level_id const& other) const -> bool
  {
    auto these_numbers = all_numbers(*this);
    auto those_numbers = all_numbers(other);
    return std::ranges::lexicographical_compare(
      these_numbers, those_numbers);
  }

  auto id_for(std::vector<std::size_t> nums) -> level_id_ptr
  {
    auto current = level_id::base_ptr();
    for (auto const num : nums) {
      current = current->make_child(num, "");
    }
    return current;
  }

  auto id_for(char const* c_str) -> level_id_ptr
  {
    std::vector<std::string> strs;
    split(strs, c_str, boost::is_any_of(":"));

    erase_if(strs, [](auto& str) -> auto { return empty(str); });

    std::vector<std::size_t> nums;
    std::ranges::transform(strs, back_inserter(nums), [](auto const& str) -> auto {
      return std::stoull(str);
    });
    return id_for(std::move(nums));
  }

  auto operator""_id(char const* c_str, std::size_t) -> level_id_ptr { return id_for(c_str); }

  auto level_id::parent() const noexcept -> level_id_ptr { return parent_; }

  auto level_id::parent(std::string const& level_name) const -> level_id_ptr
  {
    level_id_ptr parent = parent_;
    while (parent) {
      if (parent->level_name_ == level_name) {
        return parent;
      }
      parent = parent->parent_;
    }
    return nullptr;
  }

  auto level_id::to_string() const -> std::string
  {
    // FIXME: prefix needs to be adjusted esp. if a root name can be supplied by the user.
    std::string prefix{"["}; //"root: ["};
    std::string result;
    std::string suffix{"]"};

    if (number_ != -1ull) {
      result = to_string_this_level();
      auto parent = parent_;
      while (parent != nullptr and parent->number_ != -1ull) {
        result.insert(0, parent->to_string_this_level() + ", ");
        parent = parent->parent_;
      }
    }
    return prefix + result + suffix;
  }

  auto level_id::to_string_this_level() const -> std::string
  {
    if (empty(level_name_)) {
      return std::to_string(number_);
    }
    return level_name_ + ":" + std::to_string(number_);
  }

  auto operator<<(std::ostream& os, level_id const& id) -> std::ostream& { return os << id.to_string(); }
}
