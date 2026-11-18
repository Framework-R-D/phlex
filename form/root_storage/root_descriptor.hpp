// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_DESCRIPTOR_HPP
#define FORM_ROOT_STORAGE_ROOT_DESCRIPTOR_HPP

#include "storage/descriptor.hpp"

#include <memory>
#include <optional>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {

  /// Holds a ROOT file's layout descriptor as a std::map<std::string, std::string> stored under
  /// the reserved object name. ROOT ships a compiled collection proxy for that instantiation,
  /// so the bootstrap record needs no generated dictionary.
  class root_descriptor : public descriptor {
  public:
    root_descriptor(std::shared_ptr<TFile> file, std::string file_name);

    std::optional<descriptor_entries> read() override;

  protected:
    void store(descriptor_entries const& entries) override;

  private:
    std::shared_ptr<TFile> file_;
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_DESCRIPTOR_HPP
