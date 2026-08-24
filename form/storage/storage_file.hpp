// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_FILE_HPP
#define FORM_STORAGE_STORAGE_FILE_HPP

#include "istorage.hpp"

#include <string>

namespace form::detail::experimental {
  class storage_file : public i_storage_file {
  public:
    storage_file(std::string name, char mode);
    ~storage_file() override = default;

    std::string const& name() override;
    char mode() override;

    void set_attribute(std::string const& name, std::string const& value) override;

  private:
    std::string name_;
    char mode_;
  };
} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_FILE_HPP
