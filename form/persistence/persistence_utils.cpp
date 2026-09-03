#include "persistence_utils.hpp"
#include "form/config.hpp"

namespace form::detail::experimental {
  std::optional<form::experimental::config::persistence_item const> find_config_item(
    form::experimental::config::item_config const& config, std::string const& label)
  {
    auto const& items = config.get_items();
    if (label == "index") {
      return (items.empty())
               ? std::nullopt
               : std::make_optional(
                   *items
                      .begin()); //emulate how FORM did this before Phlex PR #22.  Will be fixed in a future FORM update.
    }

    return config.find_item(label);
  }
}
