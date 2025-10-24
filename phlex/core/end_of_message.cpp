#include "phlex/core/end_of_message.hpp"

#include <utility>
#include "phlex/core/fwd.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/level_hierarchy.hpp"

namespace phlex::experimental {

  end_of_message::end_of_message(end_of_message_ptr parent,
                                 level_hierarchy* hierarchy,
                                 level_id_ptr id) :
    parent_{std::move(parent)}, hierarchy_{hierarchy}, id_{std::move(id)}
  {
  }

  auto end_of_message::make_base(level_hierarchy* hierarchy, level_id_ptr id) -> end_of_message_ptr
  {
    return end_of_message_ptr{new end_of_message{nullptr, hierarchy, id}};
  }

  auto end_of_message::make_child(level_id_ptr id) -> end_of_message_ptr
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
