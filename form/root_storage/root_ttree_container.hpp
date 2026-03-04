// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TTREE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TTREE_CONTAINER_HPP

#include "storage/storage_association.hpp"

#include <memory>
#include <string>
#include <typeinfo>

class TFile;
class TTree;

namespace form::detail::experimental {

  class ROOT_TTree_ContainerImp : public Storage_Association {
  public:
    ROOT_TTree_ContainerImp(std::string const& name);
    ~ROOT_TTree_ContainerImp();

    ROOT_TTree_ContainerImp(ROOT_TTree_ContainerImp const& other) = delete;
    ROOT_TTree_ContainerImp& operator=(ROOT_TTree_ContainerImp& other) = delete;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setupWrite(std::type_info const& type = typeid(void)) override;
    void fill(void const* data) override;
    void commit() override;
    bool read(int id, void const** data, std::type_info const& type) override;

    TTree* getTTree();

  private:
    std::shared_ptr<TFile> m_tfile;
    TTree* m_tree;
  };

} //namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TTREE_CONTAINER_HPP
