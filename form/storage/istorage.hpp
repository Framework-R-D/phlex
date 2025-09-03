// Copyright (C) 2025 ...

#ifndef __ISTORAGE_H__
#define __ISTORAGE_H__

#include "core/placement.hpp"
#include "core/token.hpp"
#include "form/config.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace form::detail::experimental {

  class IStorage {
  public:
    IStorage() = default;
    virtual ~IStorage() = default;

    virtual void createContainers(
      const std::map<std::unique_ptr<Placement>, std::string>& containers,
      const form::experimental::config::tech_setting_config& settings) = 0;
    virtual void fillContainer(const Placement& plcmnt,
                               const void* data,
                               const std::string& type) = 0;
    virtual void commitContainers(const Placement& plcmnt) = 0;

    virtual int getIndex(const Token& token,
                         const std::string& id,
                         const form::experimental::config::tech_setting_config& settings) = 0;
    virtual void readContainer(const Token& token,
                               const void** data,
                               std::string& type,
                               const form::experimental::config::tech_setting_config& settings) = 0;
  };

  class IStorage_File {
  public:
    IStorage_File() = default;
    virtual ~IStorage_File() = default;

    virtual const std::string& name() = 0;
    virtual const char mode() = 0;

    virtual void setAttribute(const std::string& name, const std::string& value) = 0;
  };

  class IStorage_Container {
  public:
    IStorage_Container() = default;
    virtual ~IStorage_Container() = default;

    virtual const std::string& name() = 0;

    virtual void setFile(std::shared_ptr<IStorage_File> file) = 0;
    virtual void setupWrite(const std::string& type = "") = 0;
    virtual void fill(const void* data) = 0;
    virtual void commit() = 0;
    virtual bool read(int id, const void** data, std::string& type) = 0;

    virtual void setAttribute(const std::string& name, const std::string& value) = 0;
  };

  std::unique_ptr<IStorage> createStorage();

} // namespace form::detail::experimental

#endif
