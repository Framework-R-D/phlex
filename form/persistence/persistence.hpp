// Copyright (C) 2025 ...

#ifndef __PERSISTENCE_H__
#define __PERSISTENCE_H__

#include "ipersistence.hpp"

#include "core/placement.hpp"
#include "core/token.hpp"
#include "form/parse_config.hpp"
#include "storage/istorage.hpp"

#include <map>
#include <memory>
#include <string>

// forward declaration for form config
namespace form::experimental::config {
  class parse_config;
}

namespace form::detail::experimental {

  class Persistence : public IPersistence {
  public:
    Persistence(const form::experimental::config::parse_config& config);
    ~Persistence() = default;

    void createContainers(const std::string& creator,
                          const std::map<std::string, std::string>& products) override;
    void registerWrite(const std::string& creator,
                       const std::string& label,
                       const void* data,
                       const std::string& type) override;
    void commitOutput(const std::string& creator, const std::string& id) override;

    void read(const std::string& creator,
              const std::string& label,
              const std::string& id,
              const void** data,
              std::string& type) override;

  private:
    std::unique_ptr<Placement> getPlacement(const std::string& creator, const std::string& label);
    std::unique_ptr<Token> getToken(const std::string& creator,
                                    const std::string& label,
                                    const std::string& id);

  private:
    std::unique_ptr<IStorage> m_store;
    form::experimental::config::parse_config m_config;
  };

} // namespace form::detail::experimental

#endif
