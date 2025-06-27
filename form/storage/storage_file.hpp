// Copyright (C) 2025 ...

#ifndef __STORAGE_FILE_H__
#define __STORAGE_FILE_H__

#include "istorage.hpp"

#include <string>

class Storage_File : public IStorage_File {
public:
  Storage_File(const std::string& name, char mode);
  ~Storage_File();

  const std::string& name();

private:
  std::string m_name;
};

#endif
