#include "TClassEdit.h"

namespace form::detail::experimental {
  // Return the demangled type name
  std::string DemangleName(std::type_info const& type)
  {
    int errorCode; // NOLINT(cppcoreguidelines-init-variables) - always initialized below
    // The TClassEdit version works on both linux and Windows.
    char* demangledName = TClassEdit::DemangleTypeIdName(type, errorCode);
    if (errorCode != 0) {
      // NOTE: Instead of throwing, we could return the mangled name as a fallback.
      throw std::runtime_error("Failed to demangle type name");
    }
    std::string result(demangledName);
    std::free(demangledName);
    return result;
  }
}
