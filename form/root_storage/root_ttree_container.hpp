// Copyright (C) 2025 ...

#ifndef __ROOT_TTREE_CONTAINER_H__
#define __ROOT_TTREE_CONTAINER_H__

#include "storage/storage_association.hpp"

#include <memory>
#include <string>

class TFile;
class TTree;

namespace form::detail::experimental {

  class ROOT_TTree_ContainerImp : public Storage_Association {
  public:
    ROOT_TTree_ContainerImp(const std::string& name);
    ~ROOT_TTree_ContainerImp();

    ROOT_TTree_ContainerImp(const ROOT_TTree_ContainerImp& other) = delete;
    ROOT_TTree_ContainerImp& operator=(ROOT_TTree_ContainerImp& other) = delete;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setupWrite(const std::string& type = "") override;
    void fill(const void* data) override;
    void commit() override;
    bool read(int id, const void** data, std::string& type) override;

    TTree* getTTree();

  private:
    std::shared_ptr<TFile> m_tfile;
    TTree* m_tree;
  };

} //namespace form::detail::experimental

#endif
