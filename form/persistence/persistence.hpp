// Copyright (C) 2025 ...

#ifndef __PERSISTENCE_HPP__
#define __PERSISTENCE_HPP__

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
    Persistence(form::experimental::config::parse_config const& config);
    ~Persistence() = default;

    void createContainers(std::string const& creator,
                          std::map<std::string, std::string> const& products) override;
    void registerWrite(std::string const& creator,
                       std::string const& label,
                       void const* data,
                       std::string const& type) override;
    void commitOutput(std::string const& creator, std::string const& id) override;

    void read(std::string const& creator,
              std::string const& label,
              std::string const& id,
              void const** data,
              std::string& type) override;

  private:
    std::unique_ptr<Placement> getPlacement(std::string const& creator, std::string const& label);
    std::unique_ptr<Token> getToken(std::string const& creator,
                                    std::string const& label,
                                    std::string const& id);
    form::experimental::config::PersistenceItem const* findConfigItem(
      std::string const& label) const;
    std::string buildFullLabel(std::string_view creator, std::string_view label) const;

  private:
    std::unique_ptr<IStorage> m_store;
    form::experimental::config::parse_config m_config;
  };

} // namespace form::detail::experimental

#endif
