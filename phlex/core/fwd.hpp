#ifndef phlex_core_fwd_hpp
#define phlex_core_fwd_hpp

#include "phlex/model/fwd.hpp"
#include "phlex/utilities/async_driver.hpp"

#include <memory>

namespace phlex::experimental {
  class component;
  class consumer;
  class declared_output;
  class end_of_message;
  class generator;
  class framework_graph;
  class message_sender;
  class multiplexer;
  class products_consumer;

  using end_of_message_ptr = std::shared_ptr<end_of_message>;
  using framework_driver = async_driver<product_store_ptr>;
}

#endif // phlex_core_fwd_hpp

// Local Variables:
// mode: c++
// End:
