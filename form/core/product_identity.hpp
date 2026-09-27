// Copyright (C) 2025 ...

#ifndef FORM_CORE_PRODUCT_IDENTITY_HPP
#define FORM_CORE_PRODUCT_IDENTITY_HPP

#include <compare>
#include <string>

/* @file product_identity.hpp
 * @brief Logical identity of a written product, independent of its storage location.
 */
namespace form::detail::experimental {

  struct product_identity {
    std::string creator;
    std::string stage;
    std::string label;

    auto operator<=>(product_identity const&) const = default;
  };

} // namespace form::detail::experimental

#endif // FORM_CORE_PRODUCT_IDENTITY_HPP
