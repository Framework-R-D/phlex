// Copyright (C) 2025 ...

#include "root_tfile.hpp"

#include "TFile.h"
#include "storage/storage_file.hpp"
#include <Compression.h>
#include <memory>
#include <stdexcept>
#include <string>

using namespace form::detail::experimental;
ROOT_TFileImp::ROOT_TFileImp(std::string const& name, char mode) :
  Storage_File(name, mode), m_file(nullptr)
{
  if (mode == 'c' || mode == 'r' || mode == 'o') {
    m_file.reset(TFile::Open(name.c_str(), "RECREATE"));
  } else {
    m_file.reset(TFile::Open(name.c_str(), "READ"));
  }
}

ROOT_TFileImp::~ROOT_TFileImp() = default;

void ROOT_TFileImp::setAttribute(std::string const& key, std::string const& value)
{
  if (key == "compression") {
    using RComp = ROOT::RCompressionSetting::EAlgorithm;
    RComp::EValues compression;
    if (value == "kZLIB")
      compression = RComp::kZLIB;
    else if (value == "kLZMA")
      compression = RComp::kLZMA;
    else if (value == "kOldCompressionAlgo")
      compression = RComp::kOldCompressionAlgo;
    else if (value == "kLZ4")
      compression = RComp::kLZ4;
    else if (value == "kZSTD")
      compression = RComp::kZSTD;
    else
      compression = RComp::kUndefined;

    m_file->SetCompressionAlgorithm(compression);
  } else {
    throw std::runtime_error("ROOT_TFileImp does not recognize an attribute named " + key);
  }
}

auto ROOT_TFileImp::getTFile() -> std::shared_ptr<TFile> { return m_file; }
