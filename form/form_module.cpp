#include "phlex/module.hpp"

// FORM headers go here

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  // You can access configuration parameters...
  auto filename = config.get<std::string>("filename");

  // You can create the output node to be owned by the framework
  auto output_module = m.make<SomeFORMclass>(filename, /* any other args to SomeFORMclass ctor */);

  // Register the member function(s) that Phlex should invoke
  //   The registered member function should have the signature void(product_store const& store);
  //   (e.g., see phlex/test/products_for_output.hpp)
  output_module.output("save_data_products", &SomeFORMclass::member_function);
}
