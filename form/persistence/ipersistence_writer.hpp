// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP
#define FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP

#include "core/placement.hpp"
#include "core/token.hpp"

#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace form::experimental::config {
  struct tech_setting_config;
}

namespace form::detail::experimental {

  // Persistence is an executor: FORM owns the product configuration and hands persistence
  // fully-resolved placements. Persistence turns those into storage-layer calls and enforces the
  // token invariant; it holds no product config of its own.
  class i_persistence_writer {
  public:
    i_persistence_writer() = default;
    virtual ~i_persistence_writer() = default;

    virtual void configure_tech_settings(
      form::experimental::config::tech_setting_config const& tech_config_settings) = 0;

    // Create the given containers. FORM has already resolved each (creator, label) to a placement
    // and calls this only with containers it has not created before.
    virtual void create_containers(
      std::vector<std::pair<placement, std::type_info const*>> const& containers) = 0;

    // Write one product and return a token locating it: placement plus 0-based row (entry) number
    // Throws if backend isn't row-addressed, causing token read lookup to fail
    virtual token register_write(placement const& plcmnt,
                                 void const* data,
                                 std::type_info const& type) = 0;

    virtual void commit_output(placement const& plcmnt, std::string const& id) = 0;
  };

  std::unique_ptr<i_persistence_writer> create_persistence_writer();

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP
