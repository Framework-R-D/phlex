#include "phlex/core/dot/function_graph.hpp"
#include "phlex/core/dot/attributes.hpp"
#include "phlex/core/multiplexer.hpp"

#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>

using std::quoted;

using namespace phlex::experimental::dot;

namespace {
  void prolog(std::ostream& os)
  {
    os << "digraph test {\n\n"
       << "  // Framework-provided nodes and edges\n"
       << "  edge [fontname=Monaco, fontcolor=blue];\n"
       << "  Source [fontname=Helvetica, shape=doublecircle, style=filled, fillcolor=gray];\n"
       << "  // User-provided nodes and edges\n"
       << "  node [fontname=Monaco];\n\n";
  }

  void epilog(std::ostream& os) { os << "\n}\n"; }

  void out_node(std::ostream& os, std::string const& node_name, attributes const& attrs)
  {
    os << "  " << quoted(node_name) << " " << to_string(attrs) << ";\n";
  }

  void out_edge(std::ostream& os,
                std::string const& source_node,
                std::string const& target_node,
                attributes const& attrs)
  {
    os << "  " << quoted(source_node) << " -> " << quoted(target_node) << " " << to_string(attrs)
       << ";\n";
  }
}

namespace phlex::experimental::dot {
  void function_graph::node(std::string const& node_name, attributes const& attrs)
  {
    nodes_.push_back({node_name, attrs});
  }

  void function_graph::edge(std::string const& source_node,
                            std::string const& target_node,
                            attributes const& attrs)
  {
    edges_.push_back({source_node, target_node, attrs});
  }

  void function_graph::edges_for(std::string const& source_name,
                                 std::string const& target_name,
                                 multiplexer::named_input_ports_t const& target_ports)
  {
    for (auto const& head_port : target_ports) {
      edge(source_name,
           target_name,
           {.color = "blue",
            .fontsize = default_fontsize,
            .label = parenthesized(head_port.product_label.to_string()),
            .style = "dashed"});
    }
  }

  void function_graph::to_file(std::string const& file_prefix) const
  {
    std::ofstream out{file_prefix + "-functions.gv"};
    prolog(out);
    for (auto const& [name, attrs] : nodes_) {
      out_node(out, name, attrs);
    }
    for (auto const& [source, target, attrs] : edges_) {
      out_edge(out, source, target, attrs);
    }
    epilog(out);
  }
}
