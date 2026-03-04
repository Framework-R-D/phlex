// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_CONTAINER_HPP
#define FORM_STORAGE_STORAGE_CONTAINER_HPP

#include "istorage.hpp"

#include <memory>
#include <string>
#include <typeinfo>

namespace form::detail::experimental {

  class Storage_Container : public IStorage_Container {
  public:
    Storage_Container(std::string const& name);
    ~Storage_Container() = default;

    std::string const& name() override;

    void setFile(std::shared_ptr<IStorage_File> file) override;

    void setupWrite(std::type_info const& type = typeid(void)) override;
    void fill(void const* data) override;
    void commit() override;
    bool read(int id, void const** data, std::type_info const& type) override;

    void setAttribute(std::string const& name, std::string const& value) override;

  private:
    std::string m_name;
    std::shared_ptr<IStorage_File> m_file;
  };
} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_CONTAINER_HPP
