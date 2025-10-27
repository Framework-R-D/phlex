// Copyright (C) 2025 ...

#ifndef __STORAGE_FILE_HPP__
#define __STORAGE_FILE_HPP__

#include "istorage.hpp"

#include <string>

namespace form::detail::experimental {
  class Storage_File : public IStorage_File {
  public:
    Storage_File(std::string  name, char mode);
    ~Storage_File() = default;

    std::string const& name() override;
    char const mode() override;

    void setAttribute(std::string const& name, std::string const& value) override;

  private:
    std::string m_name;
    char m_mode;
  };
} // namespace form::detail::experimental

#endif
