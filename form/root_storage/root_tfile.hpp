// Copyright (C) 2025 ...

#ifndef __ROOT_TFILE_H__
#define __ROOT_TFILE_H__

#include "storage/storage_file.hpp"

#include <memory>
#include <string>

class TFile;

namespace form::detail::experimental {

  class ROOT_TFileImp : public Storage_File {
  public:
    ROOT_TFileImp(const std::string& name, char mode);
    ~ROOT_TFileImp() = default;

    std::shared_ptr<TFile> getTFile();

  private:
    std::shared_ptr<TFile> m_file;
  };

} // namespace form::detail::experimental

#endif
