// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_DESCRIPTOR_HPP
#define FORM_ROOT_STORAGE_ROOT_DESCRIPTOR_HPP

#include "storage/descriptor.hpp"

#include <memory>
#include <optional>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TClass;
class TFile;
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {

  /// Throws unless `cls` is a usable TClass for the descriptor's on-disk type.
  TClass const& require_descriptor_class(TClass const* cls);

  /// Throws unless `cls` carries a compiled dictionary. Writing an STL collection needs one;
  /// reading does not, since ROOT rebuilds the type from the StreamerInfo in the file.
  void require_descriptor_dictionary(TClass const& cls);

  class root_descriptor : public descriptor {
  public:
    root_descriptor(std::shared_ptr<TFile> file, std::string file_name);

  protected:
    void store(descriptor_entries const& entries) override;
    std::optional<descriptor_entries> load() override;

  private:
    std::shared_ptr<TFile> file_;
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_DESCRIPTOR_HPP
