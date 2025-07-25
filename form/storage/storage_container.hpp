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

    void setFile(std::shared_ptr<IStorage_File> file);
    void fill(const void* data, const std::string& type);
    bool read(int id, const void** data, std::string& type);

  private:
    std::string m_name;
    std::shared_ptr<IStorage_File> m_file;
    std::string m_tName;
    std::string
      m_cName; // For associative containers, with two levels (such as tree/branch), this will store column name
  };
}

#endif
