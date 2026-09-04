// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TFILE_HPP
#define FORM_ROOT_STORAGE_ROOT_TFILE_HPP

#include "storage/storage_file.hpp"

#include <memory>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {

  class root_tfile_imp : public storage_file {
  public:
    root_tfile_imp(std::string const& name, char mode);
    ~root_tfile_imp() override;

    void set_attribute(std::string const& key, std::string const& value) override;

    std::shared_ptr<TFile> get_tfile();

  private:
    std::shared_ptr<TFile> file_;
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TFILE_HPP
