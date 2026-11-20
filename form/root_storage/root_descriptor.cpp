// Copyright (C) 2025 ...

#include "root_descriptor.hpp"

#include "storage/descriptor.hpp"
#include "util/error.hpp"

#include <TClass.h>
#include <TFile.h>
#include <TKey.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace form::detail::experimental {

  TClass const& require_descriptor_class(TClass const* cls)
  {
    if (cls == nullptr) {
      error("ROOT cannot resolve the descriptor type "
            "std::vector<std::pair<std::string, std::string>>");
    }
    return *cls;
  }

  // HasDictionary() is what separates a compiled dictionary from an interpreted one.
  // GetCollectionProxy() cannot: it is non-null for both, including for the instantiation
  // TFile::WriteObject refuses to write, so it would never catch the case this guards against.
  void require_descriptor_dictionary(TClass const& cls)
  {
    if (!cls.HasDictionary()) {
      error("no compiled ROOT dictionary for "
            "std::vector<std::pair<std::string, std::string>>");
    }
  }

  namespace {
    TClass const& descriptor_class()
    {
      return require_descriptor_class(TClass::GetClass<descriptor_entries>());
    }
  }

  root_descriptor::root_descriptor(std::shared_ptr<TFile> file, std::string file_name) :
    descriptor(std::move(file_name)), file_(std::move(file))
  {
  }

  void root_descriptor::store(descriptor_entries const& entries)
  {
    if (file_ == nullptr) {
      error("no file attached for " + file_name());
    }

    require_descriptor_dictionary(descriptor_class());

    if (file_->WriteObject(&entries, std::string{descriptor_object_name}.c_str()) <= 0) {
      error("failed to write the descriptor to " + file_name());
    }
  }

  std::optional<descriptor_entries> root_descriptor::load()
  {
    if (file_ == nullptr) {
      error("no file attached for " + file_name());
    }

    std::string const object_name{descriptor_object_name};
    TKey const* const key = file_->GetKey(object_name.c_str());
    if (key == nullptr) {
      return std::nullopt;
    }

    std::string_view const stored_class{key->GetClassName()};
    std::string_view const expected_class{descriptor_class().GetName()};
    if (stored_class != expected_class) {
      error("object '" + object_name + "' in " + file_name() + " is a '" +
            std::string{stored_class} + "', not a '" + std::string{expected_class} + "'");
    }

    // Get() transfers ownership of the returned non-TObject to the caller.
    std::unique_ptr<descriptor_entries> const stored{
      file_->Get<descriptor_entries>(object_name.c_str())};
    if (!stored) {
      error("failed to read object '" + object_name + "' from " + file_name());
    }

    return std::move(*stored);
  }

} // namespace form::detail::experimental
