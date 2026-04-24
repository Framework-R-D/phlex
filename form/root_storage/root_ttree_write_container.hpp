// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TTREE_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TTREE_WRITE_CONTAINER_HPP

#include "storage/storage_write_association.hpp"

#include <memory>
#include <string>

class TFile;
class TTree;

namespace form::detail::experimental {

  class ROOT_TTree_Write_ContainerImp : public Storage_Write_Association {
  public:
    ROOT_TTree_Write_ContainerImp(std::string const& name);

    ROOT_TTree_Write_ContainerImp(ROOT_TTree_Write_ContainerImp const& other) = delete;
    ROOT_TTree_Write_ContainerImp& operator=(ROOT_TTree_Write_ContainerImp& other) = delete;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setupWrite(std::type_info const& type) override;
    void fill(void const* data) override;
    void commit() override;

    TTree* getTTree();

  private:
    // Be absolutely explicit about the ownership semantics of TTree*,
    // since ROOT nominally manages the memory of TTree objects, but in
    // practice we need to ensure objects are written to the TTree, and
    //  the TTree itself is deleted at the right time.
    struct TTreeDeleter {
      void operator()(TTree* t) const;
    };

    std::shared_ptr<TFile> m_tfile;
    std::unique_ptr<TTree, TTreeDeleter> m_tree;
  };

} //namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TTREE_WRITE_CONTAINER_HPP
