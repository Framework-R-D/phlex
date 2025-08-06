// Copyright (C) 2025 ...

#include "root_tfile.hpp"

#include "TFile.h"

using namespace form::detail::experimental;
ROOT_TFileImp::ROOT_TFileImp(const std::string& name, char mode) :
  Storage_File(name, mode), m_file(nullptr)
{
  if (mode == 'c' || mode == 'r' || mode == 'o') {
    m_file.reset(TFile::Open(name.c_str(), "RECREATE"));
  } else {
    m_file.reset(TFile::Open(name.c_str(), "READ"));
  }
}

std::shared_ptr<TFile> ROOT_TFileImp::getTFile() { return m_file; }
