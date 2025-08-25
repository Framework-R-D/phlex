// Copyright (C) 2025 ...

#ifndef __ROOT_TBRANCH_CONTAINER_H__
#define __ROOT_TBRANCH_CONTAINER_H__

#include "storage/storage_associative_container.hpp"

#include <memory>
#include <string>

class TFile;
class TTree;
class TBranch;

namespace form::detail::experimental {

  class ROOT_TBranch_ContainerImp : public Storage_Associative_Container {
  public:
    ROOT_TBranch_ContainerImp(std::string const& name);
    ~ROOT_TBranch_ContainerImp() = default;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setParent(std::shared_ptr<IStorage_Container> parent) override;

    void setupWrite(std::string const& type = "") override;
    void fill(void const* data) override;
    void commit() override;
    bool read(int id, void const** data, std::string& type) override;

  private:
    std::shared_ptr<TFile> m_tfile;
    TTree* m_tree;
    TBranch* m_branch;
  };

} // namespace form::detail::experimental

#endif
