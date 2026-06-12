// Copyright (C) 2025 ...

#include "root_tfile.hpp"

#include "TFile.h"

#include <filesystem>
#include <stdexcept>

using namespace form::detail::experimental;
ROOT_TFileImp::ROOT_TFileImp(std::string const& name, char mode) :
  Storage_File(name, mode), m_file(nullptr)
{
  if (mode == 'c' || mode == 'o') {
    // Preserve existing semantics: 'o' recreates the file, matching prior behavior
    m_file.reset(TFile::Open(name.c_str(), "RECREATE"));
  } else if (mode == 'u') {
    // 'u' explicitly means reopen/update an existing file while preserving metadata.
    if (std::filesystem::exists(name)) {
      m_file.reset(TFile::Open(name.c_str(), "UPDATE"));
    } else {
      m_file.reset(TFile::Open(name.c_str(), "RECREATE"));
    }
  } else if (mode == 'r' || mode == 'i') {
    m_file.reset(TFile::Open(name.c_str(), "READ"));
  } else {
    throw std::runtime_error(std::string("Unsupported ROOT file open mode: ") + mode);
  }

  if (!m_file || m_file->IsZombie()) {
    throw std::runtime_error("Failed to open ROOT file: " + name);
  }
}

ROOT_TFileImp::~ROOT_TFileImp() = default;

void ROOT_TFileImp::setAttribute(std::string const& key, std::string const& value)
{
  if (key == "compression") {
    using RComp = ROOT::RCompressionSetting::EAlgorithm;
    RComp::EValues compression{RComp::kUndefined};
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
    else { // leave compression as kUndefined, which will use ROOT's default
    }

    m_file->SetCompressionAlgorithm(compression);
  } else {
    throw std::runtime_error("ROOT_TFileImp does not recognize an attribute named " + key);
  }
}

std::shared_ptr<TFile> ROOT_TFileImp::getTFile() { return m_file; }
