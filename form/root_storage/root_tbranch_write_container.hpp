// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TBRANCH_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TBRANCH_WRITE_CONTAINER_HPP

#include "storage/storage_associative_write_container.hpp"

#include <memory>
#include <string>

class TFile;
class TTree;
class TBranch;

namespace form::detail::experimental {

  class ROOT_TBranch_Write_ContainerImp : public Storage_Associative_Write_Container {
  public:
    ROOT_TBranch_Write_ContainerImp(std::string const& name);
    ~ROOT_TBranch_Write_ContainerImp() = default;

    void setAttribute(std::string const& key, std::string const& value) override;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setParent(std::shared_ptr<IStorage_Write_Container> parent) override;

    void setupWrite(std::type_info const& type = typeid(void)) override;
    void fill(void const* data) override;
    void commit() override;

  private:
    std::shared_ptr<TFile> m_tfile;
    TTree* m_tree;
    TBranch* m_branch;
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TBRANCH_WRITE_CONTAINER_HPP
