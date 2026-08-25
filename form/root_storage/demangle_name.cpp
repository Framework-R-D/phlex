#include "TClassEdit.h"

#include "demangle_name.hpp"

#include <memory>

namespace {
  class char_string_holder {
  public:
    explicit char_string_holder(char* cstr) : cstr_(cstr, std::free) {}
    // Implicit conversion from char* to allow direct return of
    // the result of TClassEdit::DemangleTypeIdName.
    operator char const*() const { return cstr_.get(); } // NOLINT(google-explicit-constructor)
  private:
    std::unique_ptr<char, decltype(&std::free)> cstr_;
  };
}

namespace form::detail::experimental {
  // Return the demangled type name
  std::string demangle_name(std::type_info const& type)
  {
    int error_code{};
    // The TClassEdit version works on both linux and Windows.
    auto const demangled_name =
      char_string_holder{TClassEdit::DemangleTypeIdName(type, error_code)};
    if (error_code != 0) {
      // NOTE: Instead of throwing, we could return the mangled name as a fallback.
      throw std::runtime_error("Failed to demangle type name");
    }
    std::string result(demangled_name);
    return result;
  }
}
