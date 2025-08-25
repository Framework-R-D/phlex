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

    virtual void createContainers(const std::string& creator,
                                  const std::map<std::string, std::string>& products) = 0;
    virtual void registerWrite(const std::string& creator,
                               const std::string& label,
                               const void* data,
                               const std::string& type) = 0;
    virtual void commitOutput(const std::string& creator, const std::string& id) = 0;

    virtual void read(const std::string& creator,
                      const std::string& label,
                      const std::string& id,
                      const void** data,
                      std::string& type) = 0;
  };

  std::unique_ptr<IPersistence> createPersistence(
    const form::experimental::config::parse_config& config); // Has parameter

} // namespace form::detail::experimental

#endif
