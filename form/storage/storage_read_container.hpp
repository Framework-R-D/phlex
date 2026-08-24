// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_READ_CONTAINER_HPP
#define FORM_STORAGE_STORAGE_READ_CONTAINER_HPP

#include "istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class storage_read_container : public i_storage_read_container {
  public:
    explicit storage_read_container(std::string const& name);
    ~storage_read_container() override = default;

    std::string const& name() override;

    std::string const& top_name();

    std::string const& col_name();

    void set_file(std::shared_ptr<i_storage_file> file) override;
    void prime(std::type_info const& type) override;

    bool read(int id, void const** data, std::type_info const& type) override;
    int entries() override;

    void set_attribute(std::string const& name, std::string const& value) override;

  private:
    std::string name_;
    std::string t_name_;
    std::string c_name_;
    std::shared_ptr<i_storage_file> file_;
  };
} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_READ_CONTAINER_HPP
