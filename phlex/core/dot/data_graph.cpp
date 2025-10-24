#include "phlex/core/dot/data_graph.hpp"
#include "phlex/core/dot/attributes.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/qualified_name.hpp"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>
#include <utility>

using std::quoted;
using namespace phlex::experimental;
using namespace phlex::experimental::dot;

namespace {
  void prolog(std::ostream& os)
  {
    os << "digraph test {\n\n"
       << "  // Framework-provided nodes and edges\n"
       << "  edge [fontname=Monaco, fontcolor=blue];\n"
       << "  // User-provided nodes and edges\n"
       << "  node [fontname=Monaco, shape=plaintext];\n\n";
  }

  void epilog(std::ostream& os) { os << "\n}\n"; }

  void node_declaration(std::ostream& os, std::string const& node_name, attributes attrs)
  {
    os << "  " << quoted(parenthesized(node_name)) << " " << to_string(attrs) << ";\n";
  }

  void edge(std::ostream& os,
            std::string const& source_node,
            std::string const& target_node,
            attributes attrs)
  {
    os << "  " << quoted(parenthesized(source_node)) << " -> " << quoted(parenthesized(target_node))
       << " " << to_string(attrs) << ";\n";
  }

  auto zip_node(specified_labels const& input) -> std::string
  {
    assert(not input.empty());
    auto it = input.begin();
    auto const e = input.end();
    std::string joined = it->name.full();
    ++it;
    for (; it != e; ++it) {
      joined += ",";
      joined += it->name.full();
    }
    return joined;
  }

  auto unzip_node(qualified_names const& output) -> std::string
  {
    assert(not output.empty());
    auto it = output.begin();
    auto const e = output.end();
    std::string joined = it->name();
    ++it;
    for (; it != e; ++it) {
      joined += ",";
      joined += it->name();
    }
    return joined;
  }
}

namespace phlex::experimental::dot {

  void data_graph::add(std::string const& function_name,
                       specified_labels const& input,
                       qualified_names const& output)
  {
    std::string source_full_name{};
    std::string source_name{};
    std::string source_color{"black"};
    if (input.size() > 1ull) {
      auto zip_node_name = zip_node(input);
      for (auto const& product_name : input) {
        edges_.push_back({product_name.name.full(), zip_node_name, zip_name});
        nodes_.push_back(
          {product_name.name.full(), {.label = parenthesized(product_name.name.name())}});
      }
      source_full_name = std::move(zip_node_name);
      source_name = source_full_name;
      source_color = "gray";
    } else {
      auto const& name = input.begin()->name;
      source_full_name = name.full();
      source_name = name.name();
    }
    nodes_.push_back(
      {source_full_name, {.fontcolor = source_color, .label = parenthesized(source_name)}});

    std::string target_full_name{};
    std::string target_name{};
    std::string target_color{"black"};
    if (output.size() > 1ull) {
      auto unzip_node_name = unzip_node(output);
      for (auto const& product_name : output) {
        edges_.push_back({unzip_node_name, product_name.full(), zip_name});
        nodes_.push_back({product_name.full(), {.label = parenthesized(product_name.name())}});
      }
      target_full_name = std::move(unzip_node_name);
      target_name = target_full_name;
      target_color = "\"#a9a9a9\"";
    } else {
      auto const& name = *output.begin();
      target_full_name = name.full();
      target_name = name.name();
    }

    nodes_.push_back(
      {target_full_name, {.fontcolor = target_color, .label = parenthesized(target_name)}});
    edges_.push_back({source_full_name, target_full_name, function_name});
  }

  void data_graph::to_file(std::string const& file_prefix) const
  {
    std::ofstream out{file_prefix + "-data-pre.gv"};
    prolog(out);
    for (auto const& [source_name, target_name, edge_name] : edges_) {
      if (edge_name == zip_name) {
        edge(out,
             source_name,
             target_name,
             attributes{.color = "\"#a9a9a9\"", .fontsize = default_fontsize, .style = "dotted"});
      } else {
        edge(out,
             source_name,
             target_name,
             attributes{.color = "blue", .fontsize = default_fontsize, .label = edge_name});
      }
    }

    for (auto const& [full_name, attrs] : nodes_) {
      node_declaration(out, full_name, attrs);
    }
    epilog(out);
  }
}
