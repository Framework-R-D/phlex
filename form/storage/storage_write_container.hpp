// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_WRITE_CONTAINER_HPP
#define FORM_STORAGE_STORAGE_WRITE_CONTAINER_HPP

#include "istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class Storage_Write_Container : public IStorage_Write_Container {
  public:
    Storage_Write_Container(std::string const& name);
    ~Storage_Write_Container() = default;

    std::string const& name() override;

    void setFile(std::shared_ptr<IStorage_File> file) override;

    void setupWrite(std::type_info const& type = typeid(void)) override;
    void fill(void const* data) override;
    void commit() override;

    void setAttribute(std::string const& name, std::string const& value) override;

  private:
    std::string m_name;
    std::shared_ptr<IStorage_File> m_file;
  };
} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_WRITE_CONTAINER_HPP
