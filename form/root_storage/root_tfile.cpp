// Copyright (C) 2025 ...

#include "root_tfile.hpp"

#include "persistence/descriptor.hpp"
#include "storage/storage_file.hpp"

#include <Compression.h>
#include <TFile.h>
#include <TKey.h>
#include <TList.h>
#include <TNamed.h>
#include <TObject.h>

#include <cstddef>
#include <memory>
#include <optional>
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

  // TList::Add() takes a non-owning TObject*, so the entries are owned here and the list only
  // borrows them. Reserving up front keeps those pointers stable while the list is filled.
  std::vector<TNamed> named;
  named.reserve(entries.size());
  for (auto const& [key, value] : entries) {
    named.emplace_back(key.c_str(), value.c_str());
  }

  TList list;
  for (auto& entry : named) {
    list.Add(&entry);
  }

  if (file_->WriteObject(&list, std::string{descriptor_object_name}.c_str()) <= 0) {
    throw std::runtime_error("root_tfile_imp::write_descriptor failed to write the descriptor to " +
                             name());
  }
}

std::optional<std::vector<std::pair<std::string, std::string>>> root_tfile_imp::read_descriptor()
{
  if (file_ == nullptr) {
    throw std::runtime_error("root_tfile_imp::read_descriptor no file attached for " + name());
  }

  // Look the key up first so an absent descriptor is told apart from one that fails to read.
  // GetKey returns the highest cycle and the key stays owned by the file.
  std::string const object_name{descriptor_object_name};
  TKey* key = file_->GetKey(object_name.c_str());
  if (key == nullptr) {
    return std::nullopt;
  }

  // ReadObj hands ownership of the TList to the caller; SetOwner below extends it to the entries.
  std::unique_ptr<TObject> const object{key->ReadObj()};
  if (!object) {
    throw std::runtime_error("root_tfile_imp::read_descriptor failed to read object '" +
                             object_name + "' from " + name());
  }
  auto* list = dynamic_cast<TList*>(object.get());
  if (list == nullptr) {
    throw std::runtime_error("root_tfile_imp::read_descriptor object '" + object_name + "' in " +
                             name() + " is not a TList");
  }
  list->SetOwner(true);

  std::vector<std::pair<std::string, std::string>> entries;
  entries.reserve(static_cast<std::size_t>(list->GetSize()));
  for (TObject const* entry : *list) {
    auto const* named = dynamic_cast<TNamed const*>(entry);
    if (named == nullptr) {
      throw std::runtime_error("root_tfile_imp::read_descriptor object '" + object_name + "' in " +
                               name() + " holds an entry that is not a TNamed");
    }
    entries.emplace_back(named->GetName(), named->GetTitle());
  }
  return entries;
}

std::shared_ptr<TFile> root_tfile_imp::get_tfile() { return file_; }
