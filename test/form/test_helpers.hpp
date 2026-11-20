#ifndef TEST_FORM_TEST_HELPERS_HPP
#define TEST_FORM_TEST_HELPERS_HPP

#include "core/product_identity.hpp"

#include <string>

namespace form::test {
  /// Stage at which the FORM test fixtures write their products
  inline constexpr char const* test_stage = "test_stage";

  inline ::form::detail::experimental::product_identity product_of(
    std::string const& creator, std::string const& label, std::string const& stage = test_stage)
  {
    return ::form::detail::experimental::product_identity{
      .creator = creator, .stage = stage, .label = label};
  }
}

#endif // TEST_FORM_TEST_HELPERS_HPP
