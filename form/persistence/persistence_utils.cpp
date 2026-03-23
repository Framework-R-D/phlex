#include "persistence_utils.hpp"
#include "form/config.hpp"

namespace form::detail::experimental {
  std::optional<form::experimental::config::PersistenceItem const> findConfigItem(
    form::experimental::config::output_item_config const& config, std::string const& label)
  {
    auto const& items = config.getItems();
    if (label == "index")
      return (items.empty())
               ? std::nullopt
               : std::make_optional(
                   *items
                      .begin()); //emulate how FORM did this before Phlex PR #22.  Will be fixed in a future FORM update.

    return config.findItem(label);
  }

  std::string buildFullLabel(std::string_view creator, std::string_view label)
  {
    std::string result;
    result.reserve(creator.size() + 1 + label.size());
    result += creator;
    result += '/';
    result += label;
    return result;
  }
}
