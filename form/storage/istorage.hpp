// Copyright (C) 2025 ...

#ifndef __ISTORAGE_H__
#define __ISTORAGE_H__

#include "core/placement.hpp"
#include "core/token.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class IStorage {
  public:
    IStorage() = default;
    virtual ~IStorage() = default;

    virtual void fillContainer(const Placement& plcmnt,
                               const void* data,
                               const std::string& type) = 0;

    virtual int getIndex(const Token& token, const std::string& id) = 0;
    virtual void readContainer(const Token& token, const void** data, std::string& type) = 0;
  };

  class IStorage_File {
  public:
    virtual ~IStorage_File() = default;

    virtual const std::string& name() = 0;
  };

  class IStorage_Container {
  public:
    virtual ~IStorage_Container() {};

    virtual const std::string& name() = 0;

    virtual void setFile(std::shared_ptr<IStorage_File> file) = 0;
    virtual void fill(const void* data, const std::string& type) = 0;
    virtual bool read(int id, const void** data, std::string& type) = 0;
  };

  std::unique_ptr<IStorage> createStorage();
}

#endif
