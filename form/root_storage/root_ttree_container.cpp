// Copyright (C) 2025 ...

#include "root_ttree_container.hpp"
#include "root_tfile.hpp"

#include "TTree.h"
#include "storage/istorage.hpp"
#include "storage/storage_association.hpp"
#include <memory>
#include <stdexcept>
#include <string>

using namespace form::detail::experimental;

ROOT_TTree_ContainerImp::ROOT_TTree_ContainerImp(std::string const& name) :
  Storage_Association(name), m_tfile(nullptr), m_tree(nullptr)
{
}

ROOT_TTree_ContainerImp::~ROOT_TTree_ContainerImp()
{
  if (m_tree != nullptr) {
    m_tree->Write();
    delete m_tree;
  }
}

void ROOT_TTree_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  this->Storage_Association::setFile(file);
  auto* root_tfile_imp = dynamic_cast<ROOT_TFileImp*>(file.get());
  if (root_tfile_imp == nullptr) {
    throw std::runtime_error("ROOT_TTree_ContainerImp::setFile can't attach to non-ROOT file");
  }
  m_tfile = dynamic_cast<ROOT_TFileImp*>(file.get())->getTFile();
  return;
}

void ROOT_TTree_ContainerImp::setupWrite(std::string const& /* type*/)
{
  if (m_tfile == nullptr) {
    throw std::runtime_error("ROOT_TTree_ContainerImp::setupWrite no file attached");
  }
  if (m_tree == nullptr) {
    m_tree = m_tfile->Get<TTree>(name().c_str());
  }
  if (m_tree == nullptr) {
    m_tree = new TTree(name().c_str(), name().c_str());
    m_tree->SetDirectory(m_tfile.get());
  }
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TTree_ContainerImp::setupWrite no tree created");
  }
  return;
}

void ROOT_TTree_ContainerImp::fill(void const* /* data*/)
{
  throw std::runtime_error("ROOT_TTree_ContainerImp::fill not implemented");
}

void ROOT_TTree_ContainerImp::commit()
{
  throw std::runtime_error("ROOT_TTree_ContainerImp::commit not implemented");
}

auto ROOT_TTree_ContainerImp::read(int /* id*/, void const** /* data*/, std::string& /* type*/) -> bool
{
  throw std::runtime_error("ROOT_TTree_ContainerImp::read not implemented");
}

auto ROOT_TTree_ContainerImp::getTTree() -> TTree* { return m_tree; }
