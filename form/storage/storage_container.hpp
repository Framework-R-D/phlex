// Copyright (C) 2025 ...

#ifndef __STORAGE_CONTAINER_HPP__
#define __STORAGE_CONTAINER_HPP__

#include "istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class Storage_Container : public IStorage_Container {
  public:
    Storage_Container(std::string  name);
    ~Storage_Container() = default;

    std::string const& name() override;

    void setFile(std::shared_ptr<IStorage_File> file) override;

    void setupWrite(std::string const& type = "") override;
    void fill(void const* data) override;
    void commit() override;
    bool read(int id, void const** data, std::string& type) override;

    void setAttribute(std::string const& name, std::string const& value) override;

  private:
    std::string m_name;
    std::shared_ptr<IStorage_File> m_file;
  };
} // namespace form::detail::experimental

#endif
