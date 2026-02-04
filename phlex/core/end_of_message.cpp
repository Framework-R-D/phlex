#include "phlex/core/end_of_message.hpp"
#include "phlex/model/data_layer_hierarchy.hpp"

namespace phlex::experimental {

  end_of_message::end_of_message(end_of_message_ptr parent,
                                 data_layer_hierarchy* hierarchy,
                                 data_cell_index_ptr id) :
    parent_{parent}, hierarchy_{hierarchy}, id_{id}
  {
  }

  end_of_message_ptr end_of_message::make_base(data_layer_hierarchy* hierarchy,
                                               data_cell_index_ptr id)
  {
    return end_of_message_ptr{new end_of_message{nullptr, hierarchy, id}};
  }

  end_of_message_ptr end_of_message::make_child(data_cell_index_ptr id)
  {
    return end_of_message_ptr{new end_of_message{shared_from_this(), hierarchy_, id}};
  }

}
