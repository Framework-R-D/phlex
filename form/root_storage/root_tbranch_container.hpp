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
    ROOT_TBranch_ContainerImp(const std::string& name);
    ~ROOT_TBranch_ContainerImp();

    void setFile(std::shared_ptr<IStorage_File> file);
    void setParent(std::shared_ptr<IStorage_Container> parent);

    void setupWrite(const std::string& type = "");
    void fill(const void* data);
    void commit();
    bool read(int id, const void** data, std::string& type);

  private:
    std::shared_ptr<TFile> m_tfile;
    TTree* m_tree;
    TBranch* m_branch;
  };

} // namespace form::detail::experimental

#endif
