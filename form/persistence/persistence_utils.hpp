#ifndef FORM_PERSISTENCE_PERSISTENCE_UTILS_HPP
#define FORM_PERSISTENCE_PERSISTENCE_UTILS_HPP

#include "form/config.hpp"

#include <optional>

namespace form {
  namespace experimental::config {
    class ItemConfig;
  }

  namespace detail::experimental {
    //findConfigItem() is here and not a member of ItemConfig because of the way it handles the label "index".  Different hypothetical Persistence implementations may want to handle "index" different ways.
    std::optional<form::experimental::config::PersistenceItem const> findConfigItem(
      form::experimental::config::ItemConfig const& config, std::string const& label);

    std::string buildFullLabel(std::string_view creator, std::string_view label);
  }
}

#endif // FORM_PERSISTENCE_PERSISTENCE_UTILS_HPP
