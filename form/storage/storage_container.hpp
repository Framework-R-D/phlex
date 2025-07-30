// Copyright (C) 2025 ...

#ifndef __STORAGE_CONTAINER_H__
#define __STORAGE_CONTAINER_H__

#include "istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class Storage_Container : public IStorage_Container {
  public:
    Storage_Container(const std::string& name);
    ~Storage_Container();

    const std::string& name();

    virtual void setFile(std::shared_ptr<IStorage_File> file);

    void setupWrite(const std::string& type = "");
    void fill(const void* data);
    void commit();
    bool read(int id, const void** data, std::string& type);

  private:
    std::string m_name;
    std::shared_ptr<IStorage_File> m_file;
  };
} // namespace form::detail::experimental

#endif
