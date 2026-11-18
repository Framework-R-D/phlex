// Copyright (C) 2025 ...

#include "root_descriptor.hpp"

#include "storage/descriptor.hpp"

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

namespace form::detail::experimental {

  namespace {
    // ROOT provides a compiled collection proxy for this map type, so no generated dictionary is
    // needed for the descriptor.
    using descriptor_map = std::map<std::string, std::string>;

    TClass const& descriptor_class()
    {
      TClass const* const cls = TClass::GetClass<descriptor_map>();
      if (cls == nullptr || cls->GetCollectionProxy() == nullptr) {
        throw std::runtime_error("root_descriptor: ROOT has no dictionary for the descriptor type "
                                 "std::map<std::string, std::string>");
      }
      return *cls;
    }
  }

  root_descriptor::root_descriptor(std::shared_ptr<TFile> file, std::string file_name) :
    descriptor(std::move(file_name)), file_(std::move(file))
  {
  }

  void root_descriptor::store(descriptor_entries const& entries)
  {
    if (file_ == nullptr) {
      throw std::runtime_error("root_descriptor::store no file attached for " + file_name());
    }

    descriptor_map const stored_descriptor{entries.begin(), entries.end()};

    if (file_->WriteObject(&stored_descriptor, std::string{descriptor_object_name}.c_str()) <= 0) {
      throw std::runtime_error("root_descriptor::store failed to write the descriptor to " +
                               file_name());
    }
  }

  std::optional<descriptor_entries> root_descriptor::read()
  {
    if (file_ == nullptr) {
      throw std::runtime_error("root_descriptor::read no file attached for " + file_name());
    }

    std::string const object_name{descriptor_object_name};
    TKey const* const key = file_->GetKey(object_name.c_str());
    if (key == nullptr) {
      return std::nullopt;
    }

    std::string_view const stored_class{key->GetClassName()};
    std::string_view const expected_class{descriptor_class().GetName()};
    if (stored_class != expected_class) {
      throw std::runtime_error("root_descriptor::read object '" + object_name + "' in " +
                               file_name() + " is a '" + std::string{stored_class} + "', not a '" +
                               std::string{expected_class} + "'");
    }

    // Get() transfers ownership of the returned non-TObject to the caller.
    std::unique_ptr<descriptor_map const> const stored{
      file_->Get<descriptor_map>(object_name.c_str())};
    if (!stored) {
      throw std::runtime_error("root_descriptor::read failed to read object '" + object_name +
                               "' from " + file_name());
    }

    // Entries are returned in map key order; callers must look them up by key.
    descriptor_entries entries;
    entries.reserve(stored->size());
    for (auto const& [key_text, value] : *stored) {
      entries.emplace_back(key_text, value);
    }
    return entries;
  }

} // namespace form::detail::experimental
