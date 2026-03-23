// Copyright (C) 2025 ...

#include "root_ttree_read_container.hpp"
#include "root_tfile.hpp"

#include "TFile.h"
#include "TTree.h"

using namespace form::detail::experimental;

ROOT_TTree_Read_ContainerImp::ROOT_TTree_Read_ContainerImp(std::string const& name) :
  Storage_Read_Association(name), m_tfile(nullptr), m_tree(nullptr)
{
}

ROOT_TTree_Read_ContainerImp::~ROOT_TTree_Read_ContainerImp()
{
}

void ROOT_TTree_Read_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  this->Storage_Read_Association::setFile(file);
  ROOT_TFileImp* root_tfile_imp = dynamic_cast<ROOT_TFileImp*>(file.get());
  if (root_tfile_imp == nullptr) {
    throw std::runtime_error("ROOT_TTree_Read_ContainerImp::setFile can't attach to non-ROOT file");
  }
  m_tfile = dynamic_cast<ROOT_TFileImp*>(file.get())->getTFile();
  return;
}

bool ROOT_TTree_Read_ContainerImp::read(int /* id*/,
                                        void const** /* data*/,
                                        std::type_info const& /* type*/)
{
  throw std::runtime_error("ROOT_TTree_Read_ContainerImp::read not implemented");
}

TTree* ROOT_TTree_Read_ContainerImp::getTTree() { return m_tree; }
