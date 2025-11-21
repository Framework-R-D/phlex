#include "phlex/core/end_of_message.hpp"
#include "phlex/model/level_hierarchy.hpp"

namespace phlex::experimental {

  end_of_message::end_of_message(end_of_message_ptr parent,
                                 level_hierarchy* hierarchy,
                                 data_cell_id_ptr id) :
    parent_{parent}, hierarchy_{hierarchy}, id_{id}
  {
  }

  end_of_message_ptr end_of_message::make_base(level_hierarchy* hierarchy, data_cell_id_ptr id)
  {
    return end_of_message_ptr{new end_of_message{nullptr, hierarchy, id}};
  }

  end_of_message_ptr end_of_message::make_child(data_cell_id_ptr id)
  {
    return end_of_message_ptr{new end_of_message{shared_from_this(), hierarchy_, id}};
  }

  end_of_message::~end_of_message()
  {
    if (hierarchy_) {
      hierarchy_->increment_count(id_);
    }
  }

}
