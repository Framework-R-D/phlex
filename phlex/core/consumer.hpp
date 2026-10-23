#ifndef PHLEX_CORE_CONSUMER_HPP
#define PHLEX_CORE_CONSUMER_HPP

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/phlex_core_export.hpp"

#include <string>
#include <vector>

namespace phlex::detail {
  /// Base class of nodes that consume data products.
  ///
  /// Accessor contract: `name()` is the complete creator name. `plugin()` is that name's plugin
  /// component, which is explicitly empty (`empty_string`) for an unqualified name, such as one
  /// registered without a configuration; it is never an invalid empty `identifier`.
  /// `algorithm()` is the non-empty algorithm component.
  class PHLEX_CORE_EXPORT consumer {
  public:
    consumer(phlex::experimental::algorithm_name name, std::vector<std::string> predicates);

    phlex::experimental::algorithm_name const& name() const noexcept;
    phlex::experimental::identifier_or_empty const& plugin() const noexcept;
    phlex::experimental::identifier const& algorithm() const noexcept;
    std::vector<std::string> const& when() const noexcept;

  private:
    phlex::experimental::algorithm_name name_;
    std::vector<std::string> predicates_;
  };
}

#endif // PHLEX_CORE_CONSUMER_HPP
