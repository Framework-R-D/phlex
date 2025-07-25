// Copyright (C) 2025 ...

#include "persistence.hpp"

#include <cstring>

using namespace form::detail::experimental;

// Factory function implementation
std::unique_ptr<IPersistence> createPersistence()
{
  return std::unique_ptr<IPersistence>(new Persistence());
}

Persistence::Persistence() : m_store(nullptr) { m_store = createStorage(); }

void Persistence::registerWrite(const std::string& label, const void* data, const std::string& type)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(label);
  m_store->fillContainer(*plcmnt, data, type);
  return;
}

void Persistence::commitOutput(const std::string& label, const std::string& id)
{
  std::string index;
  auto del_pos = label.find("/");
  if (del_pos != std::string::npos) {
    index = label.substr(0, del_pos) + "/index";
  } else {
    index = label + "/index";
  }
  std::unique_ptr<Placement> plcmnt = getPlacement(index);
  const std::string& type = "std::string";
  m_store->fillContainer(*plcmnt, &id, type);
  //FIXME: Flush Containers
  return;
}

void Persistence::read(const std::string& label,
                       const std::string& id,
                       const void** data,
                       std::string& type)
{
  std::unique_ptr<Token> token = getToken(label, id);
  m_store->readContainer(*token, data, type);
  return;
}

std::unique_ptr<Placement> Persistence::getPlacement(const std::string& label)
{
  std::unique_ptr<Placement> plcmnt = std::unique_ptr<Placement>(new Placement());
  // FIXME: get parameters from configuration
  plcmnt->setTechnology(1 * 256 + 1); // Includes major/file and minor/container
  plcmnt->setFileName("toy.root");
  plcmnt->setContainerName(label);
  return plcmnt;
}

std::unique_ptr<Token> Persistence::getToken(const std::string& label, const std::string& id)
{
  std::unique_ptr<Token> token = std::unique_ptr<Token>(new Token());
  // FIXME: get parameters from configuration
  token->setTechnology(1 * 256 + 1); // Includes major/file and minor/container
  token->setFileName("toy.root");
  std::string index;
  auto del_pos = label.find("/");
  if (del_pos != std::string::npos) {
    index = label.substr(0, del_pos) + "/index";
  } else {
    index = label + "/index";
  }
  token->setContainerName(index);
  int rowId = m_store->getIndex(*token, id);
  token->setContainerName(label);
  token->setId(rowId);
  return token;
}
