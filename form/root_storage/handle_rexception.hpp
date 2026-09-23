#include <source_location>
#include <string>

namespace ROOT {
  class RException;
}

namespace form::detail::experimental {
  void handle_rexception(std::string const& message,
                         ROOT::RException const& e,
                         std::source_location const loc = std::source_location::current());
}
