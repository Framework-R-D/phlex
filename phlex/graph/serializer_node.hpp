#ifndef PHLEX_GRAPH_SERIALIZER_NODE_HPP
#define PHLEX_GRAPH_SERIALIZER_NODE_HPP

#include "phlex/utilities/sized_tuple.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <map>
#include <string>
#include <tuple>

namespace phlex::experimental {

  using token_t = int;
  using base_impl = tbb::flow::buffer_node<token_t>;
  class serializer_node : public base_impl {
  public:
    explicit serializer_node(tbb::flow::graph& g, std::string const& name) :
      base_impl{g}, name_{name}
    {
    }

    void activate()
    {
      // The serializer must not be activated until it resides in its final resting spot.
      // IOW, if a container of serializers grows, the locations of the serializers can
      // move around, introducing memory errors if try_put(...) has been attempted in a
      // different location than when it's used during the graph execution.
      try_put(1);
    }

    auto const& name() const { return name_; }

  private:
    std::string name_;
  };

  class serializers {
  public:
    explicit serializers(tbb::flow::graph& g);
    void activate();

    auto get(auto... resources) -> sized_tuple<serializer_node&, sizeof...(resources)>
    {
      // FIXME: Need to make sure there are no duplicates!
      return std::tie(get(std::string(resources))...);
    }

  private:
    serializer_node& get(std::string const& name);
    tbb::flow::graph& graph_;
    std::map<std::string, serializer_node> serializers_;
  };
}

#endif // PHLEX_GRAPH_SERIALIZER_NODE_HPP
