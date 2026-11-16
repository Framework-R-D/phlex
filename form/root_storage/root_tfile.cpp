// Copyright (C) 2025 ...

#include "root_tfile.hpp"

#include "persistence/descriptor.hpp"
#include "storage/storage_file.hpp"
#include "util/error.hpp"

#include <Compression.h>
#include <TClass.h>
#include <TFile.h>
#include <TKey.h>

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
    error("does not recognize an attribute named " + key);
  }
}

namespace {
  // ROOT provides a compiled collection proxy for this map type, so no generated dictionary is
  // needed for the descriptor.
  using descriptor_map = std::map<std::string, std::string>;

  TClass const& descriptor_class()
  {
    TClass const* const cls = TClass::GetClass<descriptor_map>();
    if (cls == nullptr || cls->GetCollectionProxy() == nullptr) {
      error("ROOT has no dictionary for the descriptor type std::map<std::string, std::string>");
    }
    return *cls;
  }
}

void root_tfile_imp::write_descriptor(
  std::vector<std::pair<std::string, std::string>> const& entries)
{
  if (file_ == nullptr) {
    error("no file attached for " + name());
  }

  // Reject duplicate keys rather than silently dropping descriptor entries.
  descriptor_map descriptor;
  for (auto const& [key, value] : entries) {
    if (!descriptor.emplace(key, value).second) {
      error("descriptor key '" + key + "' appears more than once for " + name());
    }
  }

  if (file_->WriteObject(&descriptor, std::string{descriptor_object_name}.c_str()) <= 0) {
    error("failed to write the descriptor to " + name());
  }
}

std::optional<std::vector<std::pair<std::string, std::string>>> root_tfile_imp::read_descriptor()
{
  if (file_ == nullptr) {
    error("no file attached for " + name());
  }

  std::string const object_name{descriptor_object_name};
  TKey const* const key = file_->GetKey(object_name.c_str());
  if (key == nullptr) {
    return std::nullopt;
  }

  std::string_view const stored_class{key->GetClassName()};
  std::string_view const expected_class{descriptor_class().GetName()};
  if (stored_class != expected_class) {
    error("object '" + object_name + "' in " +
          name() + " is a '" + std::string{stored_class} + "', not a '" +
          std::string{expected_class} + "'");
  }

  // Get() transfers ownership of the returned non-TObject to the caller.
  std::unique_ptr<descriptor_map const> const descriptor{
    file_->Get<descriptor_map>(object_name.c_str())};
  if (!descriptor) {
    error("failed to read object '" + object_name + "' from " + name());
  }

  // Entries are returned in map key order; callers must look them up by key.
  std::vector<std::pair<std::string, std::string>> entries;
  entries.reserve(descriptor->size());
  for (auto const& [key_text, value] : *descriptor) {
    entries.emplace_back(key_text, value);
  }
  return entries;
}

std::shared_ptr<TFile> root_tfile_imp::get_tfile() { return file_; }
