#include "handle_rexception.hpp"
#include "ROOT/RError.hxx"
#include <exception>

namespace form::detail::experimental {
  void handle_rexception(std::string const& message,
                         ROOT::RException const& e,
                         std::source_location const loc)
  {
    std::throw_with_nested(std::runtime_error(std::string(loc.function_name()) + ": " + message +
                                              " because:\n" + e.what()));
  }
}
