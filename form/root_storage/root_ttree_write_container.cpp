// Copyright (C) 2025 ...

#include "root_ttree_write_container.hpp"
#include "root_tfile.hpp"

#include "TFile.h"
#include "TTree.h"

#include <gsl/pointers>

using namespace form::detail::experimental;

ROOT_TTree_Write_ContainerImp::ROOT_TTree_Write_ContainerImp(std::string const& name) :
  Storage_Write_Association(name), m_tfile(nullptr), m_tree(nullptr)
{
}

void ROOT_TTree_Write_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  this->Storage_Write_Association::setFile(file);
  ROOT_TFileImp* root_tfile_imp = dynamic_cast<ROOT_TFileImp*>(file.get());
  if (root_tfile_imp == nullptr) {
    throw std::runtime_error(
      "ROOT_TTree_Write_ContainerImp::setFile can't attach to non-ROOT file");
  }
  m_tfile = dynamic_cast<ROOT_TFileImp*>(file.get())->getTFile();
  return;
}

void ROOT_TTree_Write_ContainerImp::setupWrite(std::type_info const& /* type*/)
{
  if (m_tfile == nullptr) {
    throw std::runtime_error("ROOT_TTree_Write_ContainerImp::setupWrite no file attached");
  }
  if (m_tree == nullptr) {
    m_tree.reset(m_tfile->Get<TTree>(name().c_str()));
  }
  if (m_tree == nullptr) {
    m_tree.reset(gsl::owner<TTree*>{new TTree(name().c_str(), name().c_str())});
    m_tree->SetDirectory(m_tfile.get());
  }
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TTree_Write_ContainerImp::setupWrite no tree created");
  }
  return;
}

void ROOT_TTree_Write_ContainerImp::fill(void const* /* data*/)
{
  throw std::runtime_error("ROOT_TTree_Write_ContainerImp::fill not implemented");
}

void ROOT_TTree_Write_ContainerImp::commit()
{
  throw std::runtime_error("ROOT_TTree_Write_ContainerImp::commit not implemented");
}

TTree* ROOT_TTree_Write_ContainerImp::getTTree() { return m_tree.get(); }

void ROOT_TTree_Write_ContainerImp::TTreeDeleter::operator()(gsl::owner<TTree*> t) const
{
  if (t) {
    t->GetDirectory()->WriteTObject(t);
    delete t;
  }
}
