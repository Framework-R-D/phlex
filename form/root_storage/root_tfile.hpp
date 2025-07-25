// Copyright (C) 2025 ...

#ifndef __ROOT_TFILE_H__
#define __ROOT_TFILE_H__

#include "storage/istorage.hpp"

#include <memory>
#include <string>

class TFile;

namespace form::detail::experimental {
  class ROOT_TFileImp : public IStorage_File {
  public:
    ROOT_TFileImp(const std::string& name, char mode);
    ~ROOT_TFileImp();

    const std::string& name();
    std::shared_ptr<TFile> getTFile();

  private:
    std::string m_name;
    char m_mode;

    std::shared_ptr<TFile> m_file;
  };
}

#endif
