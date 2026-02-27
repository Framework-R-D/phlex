#ifndef FORM_FORM_PRODUCT_WITH_NAME_HPP
#define FORM_FORM_PRODUCT_WITH_NAME_HPP

#include <string>
#include <typeinfo>

namespace form::experimental
{
  struct product_with_name {
    std::string label;
    void const* data;
    std::type_info const* type;
  };
}

#endif // FORM_FORM_PRODUCT_WITH_NAME_HPP
