// Copyright (C) 2025 ...

#include "root_tfile.hpp"

#include "persistence/descriptor.hpp"
#include "storage/storage_file.hpp"

#include <Compression.h>
#include <TFile.h>
#include <TList.h>
#include <TNamed.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    using rcomp = ROOT::RCompressionSetting::EAlgorithm;
    rcomp::EValues compression{rcomp::kUndefined};
    if (value == "kZLIB") {
      compression = rcomp::kZLIB;
    } else if (value == "kLZMA") {
      compression = rcomp::kLZMA;
    } else if (value == "kOldCompressionAlgo") {
      compression = rcomp::kOldCompressionAlgo;
    } else if (value == "kLZ4") {
      compression = rcomp::kLZ4;
    } else if (value == "kZSTD") {
      compression = rcomp::kZSTD;
    } else { // leave compression as kUndefined, which will use ROOT's default
    }

    file_->SetCompressionAlgorithm(compression);
  } else {
    throw std::runtime_error("root_tfile_imp does not recognize an attribute named " + key);
  }
}

void root_tfile_imp::write_descriptor(
  std::vector<std::pair<std::string, std::string>> const& entries)
{
  if (file_ == nullptr) {
    throw std::runtime_error("root_tfile_imp::write_descriptor no file attached for " + name());
  }

  TList list;
  list.SetOwner(true);
  for (auto const& [key, value] : entries) {
    list.Add(new TNamed(key.c_str(), value.c_str()));
  }

  if (file_->WriteObject(&list, std::string{descriptor_object_name}.c_str()) <= 0) {
    throw std::runtime_error("root_tfile_imp::write_descriptor failed to write the descriptor to " +
                             name());
  }
}

std::shared_ptr<TFile> root_tfile_imp::get_tfile() { return file_; }
