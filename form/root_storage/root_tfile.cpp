// Copyright (C) 2025 ...

#include "root_tfile.hpp"

#include "TFile.h"

using namespace form::detail::experimental;
root_tfile_imp::root_tfile_imp(std::string const& name, char mode) :
  storage_file(name, mode), file_(nullptr)
{
  if (mode == 'c' || mode == 'r' || mode == 'o') {
    file_.reset(TFile::Open(name.c_str(), "RECREATE"));
  } else {
    file_.reset(TFile::Open(name.c_str(), "READ"));
  }
}

root_tfile_imp::~root_tfile_imp() = default;

void root_tfile_imp::set_attribute(std::string const& key, std::string const& value)
{
  if (key == "compression") {
    using RComp = ROOT::RCompressionSetting::EAlgorithm; // NOLINT(readability-identifier-naming)
    RComp::EValues compression{RComp::kUndefined};
    if (value == "kZLIB") {
      compression = RComp::kZLIB;
    } else if (value == "kLZMA") {
      compression = RComp::kLZMA;
    } else if (value == "kOldCompressionAlgo") {
      compression = RComp::kOldCompressionAlgo;
    } else if (value == "kLZ4") {
      compression = RComp::kLZ4;
    } else if (value == "kZSTD") {
      compression = RComp::kZSTD;
    } else { // leave compression as kUndefined, which will use ROOT's default
    }

    file_->SetCompressionAlgorithm(compression);
  } else {
    throw std::runtime_error("root_tfile_imp does not recognize an attribute named " + key);
  }
}

std::shared_ptr<TFile> root_tfile_imp::get_tfile() { return file_; }
