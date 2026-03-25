// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TBRANCH_READ_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TBRANCH_READ_CONTAINER_HPP

#include "storage/storage_read_container.hpp"

#include <memory>
#include <string>

class TFile;
class TTree;
class TBranch;

namespace form::detail::experimental {

  class ROOT_TBranch_Read_ContainerImp : public Storage_Read_Container {
  public:
    ROOT_TBranch_Read_ContainerImp(std::string const& name);
    ~ROOT_TBranch_Read_ContainerImp() = default;

    void setFile(std::shared_ptr<IStorage_File> file) override;

    bool read(int id, void const** data, std::type_info const& type) override;

  private:
    std::shared_ptr<TFile> m_tfile;
    TTree* m_tree;
    TBranch* m_branch;
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TBRANCH_READ_CONTAINER_HPP
