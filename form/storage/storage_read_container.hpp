// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_READ_CONTAINER_HPP
#define FORM_STORAGE_STORAGE_READ_CONTAINER_HPP

#include "istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class Storage_Read_Container : public IStorage_Read_Container {
  public:
    Storage_Read_Container(std::string const& name);
    ~Storage_Read_Container() override = default;

    std::string const& name() override;

    std::string const& top_name();

    std::string const& col_name();

    void setFile(std::shared_ptr<IStorage_File> file) override;

    bool read(int id, void const** data, std::type_info const& type) override;

    void setAttribute(std::string const& name, std::string const& value) override;

  private:
    std::string m_name;
    std::string m_tName;
    std::string m_cName;
    std::shared_ptr<IStorage_File> m_file;
  };
} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_READ_CONTAINER_HPP
