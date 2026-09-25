#include <source_location>
#include <string>

namespace ROOT { // NOLINT
  class RException; // NOLINT
}

namespace form::detail::experimental {
  void handle_rexception(std::string const& message,
                         ROOT::RException const& e,
                         std::source_location loc = std::source_location::current());
}
