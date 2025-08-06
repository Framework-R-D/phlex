// Copyright (C) 2025 ...

#include "persistence.hpp"

#include <cstring>
#include <utility>

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IPersistence> createPersistence(
    const form::experimental::config::parse_config& config) //factory takes form config
  {
    return std::unique_ptr<IPersistence>(new Persistence(config));
  }
} // namespace form::detail::experimental

Persistence::Persistence(const form::experimental::config::parse_config& config) :
  m_store(nullptr), m_config(config) // constructor takes form config
{
  m_store = createStorage();
}

void Persistence::createContainers(const std::string& creator,
                                   const std::map<std::string, std::string>& products)
{
  std::map<std::unique_ptr<Placement>, std::string> containers;
  for (const auto& [label, type] : products) {
    containers.insert(std::make_pair(getPlacement(creator, label), type));
  }
  containers.insert(std::make_pair(getPlacement(creator, "/index"), "std::string"));
  m_store->createContainers(containers);
  return;
}

void Persistence::registerWrite(const std::string& creator,
                                const std::string& label,
                                const void* data,
                                const std::string& type)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, label);
  m_store->fillContainer(*plcmnt, data, type);
  return;
}

void Persistence::commitOutput(const std::string& creator, const std::string& id)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, "/index");
  m_store->fillContainer(*plcmnt, &id, "std::string");
  //FIXME: Flush Containers
  m_store->commitContainers(*plcmnt);
  return;
}

void Persistence::read(const std::string& creator,
                       const std::string& label,
                       const std::string& id,
                       const void** data,
                       std::string& type)
{
  std::unique_ptr<Token> token = getToken(creator, label, id);
  m_store->readContainer(*token, data, type);
  return;
}

std::unique_ptr<Placement> Persistence::getPlacement(const std::string& creator,
                                                     const std::string& label)
{
  //use config to determine values
  std::string file_name;
  int technology;

  std::string product_name = label;

  // Special handling for index containers
  bool found = false;
  if (product_name == "/index") {
    // Index containers use same file/technology as their creator
    // Find any item from this creator to get file/technology
    for (const auto& item : m_config.getItems()) {
      if (item.product_name != "index") {
        file_name = item.file_name;
        technology = item.technology;
        found = true;
        break;
      }
    }
  } else {
    // Find exact match in config for regular data products
    for (const auto& item : m_config.getItems()) {
      if (item.product_name == product_name) {
        file_name = item.file_name;
        technology = item.technology;
        found = true;
        break;
      }
    }
  }
  if (!found) {
    throw std::runtime_error("No configuration found for product: " + product_name +
                             " from creator: " + creator);
  }

  std::string full_label;
  if (product_name == "/index") {
    full_label = creator + "/index";
  } else {
    full_label = creator + "/" + product_name;
  }

  std::unique_ptr<Placement> plcmnt =
    std::unique_ptr<Placement>(new Placement(file_name, full_label, technology));
  return plcmnt;
}

std::unique_ptr<Token> Persistence::getToken(const std::string& creator,
                                             const std::string& label,
                                             const std::string& id)
{
  // Full label and index label construction
  std::string full_label = creator + "/" + label;
  std::string index_label = creator + "/index";

  // Get parameters from configuration
  std::string product_name = label;

  std::string file_name;
  int technology;
  bool found = false;
  for (const auto& item : m_config.getItems()) {
    if (item.product_name == product_name) {
      file_name = item.file_name;
      technology = item.technology;
      found = true;
      break;
    }
  }

  if (!found) {
    throw std::runtime_error("No configuration found for product: " + product_name +
                             " from creator: " + creator);
  }

  std::unique_ptr<Token> index_token =
    std::unique_ptr<Token>(new Token(file_name, index_label, technology));
  int rowId = m_store->getIndex(*index_token, id);
  std::unique_ptr<Token> token =
    std::unique_ptr<Token>(new Token(file_name, full_label, technology, rowId));
  return token;
}
