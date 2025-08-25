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
    ~Storage_Container() = default;

    const std::string& name() override;

    void setFile(std::shared_ptr<IStorage_File> file) override;

    void setupWrite(const std::string& type = "") override;
    void fill(const void* data) override;
    void commit() override;
    bool read(int id, const void** data, std::string& type) override;

  private:
    std::string m_name;
    std::shared_ptr<IStorage_File> m_file;
  };
} // namespace form::detail::experimental

#endif
