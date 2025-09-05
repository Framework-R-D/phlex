// Copyright (C) 2025 ...

#ifndef __ISTORAGE_HPP__
#define __ISTORAGE_HPP__

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
      std::map<std::unique_ptr<Placement>, std::string> const& containers,
      form::experimental::config::tech_setting_config const& settings) = 0;
    virtual void fillContainer(Placement const& plcmnt,
                               void const* data,
                               std::string const& type) = 0;
    virtual void commitContainers(Placement const& plcmnt) = 0;

    virtual int getIndex(Token const& token,
                         std::string const& id,
                         form::experimental::config::tech_setting_config const& settings) = 0;
    virtual void readContainer(Token const& token,
                               void const** data,
                               std::string& type,
                               form::experimental::config::tech_setting_config const& settings) = 0;
  };

  class IStorage_File {
  public:
    IStorage_File() = default;
    virtual ~IStorage_File() = default;

    virtual std::string const& name() = 0;
    virtual char const mode() = 0;

    virtual void setAttribute(std::string const& name, std::string const& value) = 0;
  };

  class IStorage_Container {
  public:
    IStorage_Container() = default;
    virtual ~IStorage_Container() = default;

    virtual std::string const& name() = 0;

    virtual void setFile(std::shared_ptr<IStorage_File> file) = 0;
    virtual void setupWrite(std::string const& type = "") = 0;
    virtual void fill(void const* data) = 0;
    virtual void commit() = 0;
    virtual bool read(int id, void const** data, std::string& type) = 0;

    virtual void setAttribute(std::string const& name, std::string const& value) = 0;
  };

  std::unique_ptr<IStorage> createStorage();

} // namespace form::detail::experimental

#endif
