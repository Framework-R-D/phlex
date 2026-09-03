#include "phlex/source.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex;

namespace {
  class empty_source final : public source {
    detail::provider_bundles create_providers(product_selector const&) override { return {}; }
  };
}

TEST_CASE("source::indices is empty by default", "[core]")
{
  std::unique_ptr<phlex::source> src = std::make_unique<empty_source>();
  auto indices = src->indices();
  CHECK(indices.begin() == indices.end());
}
