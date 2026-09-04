// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_WRITE_CONTAINER_HPP
#define FORM_STORAGE_STORAGE_WRITE_CONTAINER_HPP

#include "istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class storage_write_container : public i_storage_write_container {
  public:
    explicit storage_write_container(std::string name);
    ~storage_write_container() override = default;

    std::string const& name() override;

    void set_file(std::shared_ptr<i_storage_file> file) override;

    void setup_write(std::type_info const& type = typeid(void)) override;
    std::uint64_t fill(void const* data) override;
    void commit() override;

    void set_attribute(std::string const& name, std::string const& value) override;

  private:
    std::string name_;
    std::shared_ptr<i_storage_file> file_;
  };
} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_WRITE_CONTAINER_HPP
