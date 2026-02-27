// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TTREE_READ_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TTREE_READ_CONTAINER_HPP

#include "storage/storage_read_association.hpp"

#include <memory>
#include <string>

class TFile;
class TTree;

namespace form::detail::experimental {

  class ROOT_TTree_Read_ContainerImp : public Storage_Read_Association {
  public:
    ROOT_TTree_Read_ContainerImp(std::string const& name);
    ~ROOT_TTree_Read_ContainerImp();

    ROOT_TTree_Read_ContainerImp(ROOT_TTree_Read_ContainerImp const& other) = delete;
    ROOT_TTree_Read_ContainerImp& operator=(ROOT_TTree_Read_ContainerImp& other) = delete;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    bool read(int id, void const** data, std::type_info const& type) override;

    TTree* getTTree();

  private:
    std::shared_ptr<TFile> m_tfile;
    TTree* m_tree;
  };

} //namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TTREE_READ_CONTAINER_HPP
