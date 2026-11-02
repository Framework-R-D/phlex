// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_FILE_HPP
#define FORM_STORAGE_STORAGE_FILE_HPP

#include "istorage.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace form::detail::experimental {
  class storage_file : public i_storage_file {
  public:
    storage_file(std::string name, char mode);
    ~storage_file() override = default;

    std::string const& name() override;
    char mode() override;

    void set_attribute(std::string const& name, std::string const& value) override;

    void write_descriptor(std::vector<std::pair<std::string, std::string>> const& entries) override;
    std::optional<std::vector<std::pair<std::string, std::string>>> read_descriptor() override;

  private:
    std::string name_;
    char mode_;
    std::optional<std::vector<std::pair<std::string, std::string>>> descriptor_;
  };
} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_FILE_HPP
