// Copyright (C) 2025 ...

#ifndef __STORAGE_FILE_H__
#define __STORAGE_FILE_H__

#include "istorage.hpp"

#include <string>

namespace form::detail::experimental {
  class Storage_File : public IStorage_File {
  public:
    Storage_File(const std::string& name, char mode);
    ~Storage_File() = default;

    const std::string& name() override;
    const char mode() override;

  private:
    std::string m_name;
    char m_mode;
  };
} // namespace form::detail::experimental

#endif
