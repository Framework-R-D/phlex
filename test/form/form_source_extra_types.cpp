#include "form/form_source_type_registry.hpp"
#include "phlex/module.hpp"

using namespace phlex;

namespace {
  void register_extra_form_types_once()
  {
    static bool registered = false;
    if (registered) {
      return;
    }

    // Example extension point: these types are not in the built-in registry.
    // Users can add their custom types here as needed.
    form::experimental::register_form_vector_product_type<short>("std::vector<short>");
    form::experimental::register_form_vector_product_type<unsigned short>(
      "std::vector<unsigned short>");
    form::experimental::register_form_vector_product_type<unsigned char>(
      "std::vector<unsigned char>");

    registered = true;
  }
}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  (void)m;
  (void)config;
  register_extra_form_types_once();
}
