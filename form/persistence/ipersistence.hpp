// Copyright (C) 2025 ...

#ifndef __IPERSISTENCE_H__
#define __IPERSISTENCE_H__

#include <map>
#include <memory>
#include <string>

namespace form::experimental::config {
  class parse_config;
}

namespace form::detail::experimental {

  class IPersistence {
  public:
    IPersistence() {};
    virtual ~IPersistence() = default;

    virtual void createContainers(std::string const& creator,
                                  std::map<std::string, std::string> const& products) = 0;
    virtual void registerWrite(std::string const& creator,
                               std::string const& label,
                               void const* data,
                               std::string const& type) = 0;
    virtual void commitOutput(std::string const& creator, std::string const& id) = 0;

    virtual void read(std::string const& creator,
                      std::string const& label,
                      std::string const& id,
                      void const** data,
                      std::string& type) = 0;
  };

  std::unique_ptr<IPersistence> createPersistence(
    form::experimental::config::parse_config const& config); // Has parameter

} // namespace form::detail::experimental

#endif
