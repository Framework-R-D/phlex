#include <string>
#include <typeinfo>

namespace form::detail::experimental {
  // Return the demangled type name
  std::string DemangleName(std::type_info const& type);
}
