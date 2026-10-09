#ifndef PHLEX_CORE_DETAIL_JOIN_SUPPORT_HPP
#define PHLEX_CORE_DETAIL_JOIN_SUPPORT_HPP

#include "phlex/core/detail/repeater_node.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/phlex_core_export.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace phlex::detail::internal {

  // Owns the arity-independent data repeaters and their index-router metadata.
  class PHLEX_CORE_EXPORT join_support {
  public:
    join_support(tbb::flow::graph& g,
                 std::string const& node_name,
                 std::vector<phlex::experimental::identifier> const& layers);
    join_support(tbb::flow::graph& g,
                 phlex::experimental::algorithm_name const& node_name,
                 std::vector<phlex::experimental::identifier> const& layers,
                 named_index_port partition);
    ~join_support();

    join_support(join_support const&) = delete;
    join_support& operator=(join_support const&) = delete;
    join_support(join_support&&) = delete;
    join_support& operator=(join_support&&) = delete;

    bool has_repeaters() const noexcept;
    repeater_node& repeater(std::size_t index);
    named_index_ports const& index_ports() const noexcept;

  private:
    void initialize(tbb::flow::graph& g,
                    phlex::experimental::algorithm_name const& node_name,
                    std::vector<phlex::experimental::identifier> const& layers,
                    std::optional<named_index_port> partition);

    std::vector<std::unique_ptr<repeater_node>> repeaters_;
    named_index_ports index_ports_;
  };
}

#endif // PHLEX_CORE_DETAIL_JOIN_SUPPORT_HPP
