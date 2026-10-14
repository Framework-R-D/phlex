#include "phlex/core/registrar.hpp"

#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/provider_node.hpp"

#include <fmt/format.h>

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  template <typename Ptr>
  registrar<Ptr>::registrar(nodes& node_map, std::vector<std::string>& errors) :
    nodes_{&node_map}, errors_{&errors}
  {
  }

  template <typename Ptr>
  registrar<Ptr>::registrar(registrar&& other) noexcept :
    nodes_{other.nodes_},
    errors_{other.errors_},
    creator_{std::exchange(other.creator_, node_creator{})},
    predicates_{std::exchange(other.predicates_, std::nullopt)},
    output_product_suffixes_{std::move(other.output_product_suffixes_)}
  {
  }

  template <typename Ptr>
  registrar<Ptr>& registrar<Ptr>::operator=(registrar&& other) noexcept
  {
    nodes_ = other.nodes_;
    errors_ = other.errors_;
    creator_ = std::exchange(other.creator_, node_creator{});
    predicates_ = std::exchange(other.predicates_, std::nullopt);
    output_product_suffixes_ = std::move(other.output_product_suffixes_);
    return *this;
  }

  template <typename Ptr>
  bool registrar<Ptr>::has_predicates() const
  {
    return predicates_.has_value();
  }

  template <typename Ptr>
  void registrar<Ptr>::set_creator(node_creator creator)
  {
    creator_ = std::move(creator);
  }

  template <typename Ptr>
  void registrar<Ptr>::set_predicates(std::optional<std::vector<std::string>> predicates)
  {
    predicates_ = std::move(predicates);
  }

  template <typename Ptr>
  void registrar<Ptr>::set_output_product_suffixes(std::vector<std::string> output_product_suffixes)
  {
    create_node(std::move(output_product_suffixes));
  }

  template <typename Ptr>
  registrar<Ptr>::~registrar() noexcept(false)
  {
    if (creator_) {
      create_node(std::move(output_product_suffixes_));
    }
  }

  template <typename Ptr>
  std::vector<std::string> registrar<Ptr>::release_predicates()
  {
    return std::move(predicates_).value_or(std::vector<std::string>{});
  }

  template <typename Ptr>
  void registrar<Ptr>::create_node(std::vector<std::string> output_product_suffixes)
  {
    assert(creator_);
    auto create = std::exchange(creator_, node_creator{});
    auto ptr = create(release_predicates(), std::move(output_product_suffixes));
    auto name = ptr->name().to_string();
    auto [_, inserted] = nodes_->try_emplace(name, std::move(ptr));
    if (not inserted) {
      internal::add_to_error_messages(*errors_, "Node", name);
    }
  }

  template class registrar<declared_fold_ptr>;
  template class registrar<declared_observer_ptr>;
  template class registrar<declared_output_ptr>;
  template class registrar<declared_predicate_ptr>;
  template class registrar<declared_transform_ptr>;
  template class registrar<declared_unfold_ptr>;
  template class registrar<provider_node_ptr>;
}

namespace phlex::detail::internal {
  void add_to_error_messages(std::vector<std::string>& errors,
                             std::string const& entity,
                             std::string const& name)
  {
    errors.push_back(fmt::format("{} with name '{}' already exists", entity, name));
  }
}
