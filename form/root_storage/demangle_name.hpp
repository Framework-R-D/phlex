#ifndef FORM_ROOT_STORAGE_DEMANGLE_NAME_HPP
#define FORM_ROOT_STORAGE_DEMANGLE_NAME_HPP

#include <string>
#include <typeinfo>

namespace form::detail::experimental {
  // Return the demangled type name
  std::string demangle_name(std::type_info const& type);
}

#endif // FORM_ROOT_STORAGE_DEMANGLE_NAME_HPP
