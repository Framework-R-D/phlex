// Copyright (C) 2025 ...

#ifndef __ROOT_TTREE_CONTAINER_H__
#define __ROOT_TTREE_CONTAINER_H__

#include "storage/istorage.hpp"

#include <memory>
#include <string>

class TFile;
class TTree;
class TBranch;

namespace form::detail::experimental {

  class ROOT_TTree_ContainerImp : public IStorage_Container {
  public:
    ROOT_TTree_ContainerImp(const std::string& name);
    ~ROOT_TTree_ContainerImp();

    const std::string& name();

    void setFile(std::shared_ptr<IStorage_File> file);
    void fill(const void* data, const std::string& type);
    bool read(int id, const void** data, std::string& type);

  private:
    std::string m_name;
    std::string m_treeName;
    std::string m_branchName;

    std::shared_ptr<TFile> m_file;
    TTree* m_tree;
    bool m_ownsTree;
    TBranch* m_branch;
  };
}

#endif
