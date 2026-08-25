#ifndef FORM_PERSISTENCE_PERSISTENCE_UTILS_HPP
#define FORM_PERSISTENCE_PERSISTENCE_UTILS_HPP

#include "form/config.hpp"

#include <optional>

namespace form {
  namespace experimental::config {
    class item_config;
  }

  namespace detail::experimental {
    //find_config_item() is here and not a member of item_config because of the way it handles the label "index".  Different hypothetical Persistence implementations may want to handle "index" different ways.
    std::optional<form::experimental::config::persistence_item const> find_config_item(
      form::experimental::config::item_config const& config, std::string const& label);

    std::string build_full_label(std::string_view creator, std::string_view label);
  }
}

#endif // FORM_PERSISTENCE_PERSISTENCE_UTILS_HPP
