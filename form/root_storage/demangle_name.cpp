#include "TClassEdit.h"

#include "demangle_name.hpp"

#include <memory>

namespace {
  class char_string_holder {
  public:
    char_string_holder(char* cstr) : m_cstr(cstr, std::free) {}
    operator char const*() const { return m_cstr.get(); }

  private:
    std::unique_ptr<char, decltype(&std::free)> m_cstr;
  };
}

namespace form::detail::experimental {
  // Return the demangled type name
  std::string DemangleName(std::type_info const& type)
  {
    int errorCode{};
    // The TClassEdit version works on both linux and Windows.
    auto const demangledName = char_string_holder{TClassEdit::DemangleTypeIdName(type, errorCode)};
    if (errorCode != 0) {
      // NOTE: Instead of throwing, we could return the mangled name as a fallback.
      throw std::runtime_error("Failed to demangle type name");
    }
    std::string result(demangledName);
    return result;
  }
}
