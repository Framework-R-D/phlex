// Copyright (C) 2025 ...

#ifndef __ISTORAGE_H__
#define __ISTORAGE_H__

#include "core/placement.hpp"
#include "core/token.hpp"

#include <map>
#include <memory>
#include <string>

namespace form::detail::experimental {

  class IStorage {
  public:
    IStorage() = default;
    virtual ~IStorage() = default;

    virtual void createContainers(
      const std::map<std::unique_ptr<Placement>, std::string>& containers) = 0;
    virtual void fillContainer(const Placement& plcmnt,
                               const void* data,
                               const std::string& type) = 0;
    virtual void commitContainers(const Placement& plcmnt) = 0;

    virtual int getIndex(const Token& token, const std::string& id) = 0;
    virtual void readContainer(const Token& token, const void** data, std::string& type) = 0;
  };

  class IStorage_File {
  public:
    IStorage_File() = default;
    virtual ~IStorage_File() = default;

    virtual const std::string& name() = 0;
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
  };

  std::unique_ptr<IStorage> createStorage();

} // namespace form::detail::experimental

#endif
