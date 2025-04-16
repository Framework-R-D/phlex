#ifndef phlex_core_dot_data_graph_hpp
#define phlex_core_dot_data_graph_hpp

#include "phlex/core/dot/attributes.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/qualified_name.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace phlex::experimental::dot {
  inline std::string const zip_name{"[zip]"};

  class data_graph {
  public:
    void add(std::string const& function_name, specified_labels input, qualified_names output);
    void to_file(std::string const& file_prefix) const;

  private:
    struct product_edge {
      std::string source;
      std::string target;
      std::string edge;
    };
    struct product_node {
      std::string full_name;
      dot::attributes attrs;
    };
    std::vector<product_edge> edges_;
    std::vector<product_node> nodes_;
  };
}
#endif // phlex_core_dot_data_graph_hpp
