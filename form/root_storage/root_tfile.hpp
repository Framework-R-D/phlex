// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TFILE_HPP
#define FORM_ROOT_STORAGE_ROOT_TFILE_HPP

#include "storage/storage_file.hpp"

#include <memory>
#include <string>

class TFile;

namespace form::detail::experimental {

  class ROOT_TFileImp : public Storage_File {
  public:
    ROOT_TFileImp(std::string const& name, char mode);
    ~ROOT_TFileImp();

    void setAttribute(std::string const& key, std::string const& value) override;

    std::shared_ptr<TFile> getTFile();

  private:
    std::shared_ptr<TFile> m_file;
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TFILE_HPP
