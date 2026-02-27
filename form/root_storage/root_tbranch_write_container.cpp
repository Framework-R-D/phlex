// Copyright (C) 2025 ...

#include "root_tbranch_write_container.hpp"
#include "root_tfile.hpp"
#include "root_ttree_write_container.hpp"
#include "demangle_name.hpp"

#include "TBranch.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TTree.h"

#include <unordered_map>

namespace {
  //Type name conversion based on https://root.cern.ch/doc/master/classTTree.html#ac1fa9466ce018d4aa739b357f981c615
  //An empty leaf list defaults to Float_t
  std::unordered_map<std::string, std::string> typeNameToLeafList = {{"int", "/I"},
                                                                     {"unsigned int", "/i"},
                                                                     {"float", "/F"},
                                                                     {"double", "/D"},
                                                                     {"short int", "/S"},
                                                                     {"unsigned short", "/s"},
                                                                     {"long int", "/L"},
                                                                     {"unsigned long int", "/l"},
                                                                     {"bool", "/O"}};
}

using namespace form::detail::experimental;

ROOT_TBranch_Write_ContainerImp::ROOT_TBranch_Write_ContainerImp(std::string const& name) :
  Storage_Associative_Write_Container(name), m_tfile(nullptr), m_tree(nullptr), m_branch(nullptr)
{
}

void ROOT_TBranch_Write_ContainerImp::setAttribute(std::string const& key, std::string const& value)
{
  if (key == "auto_flush") {
    m_tree->SetAutoFlush(std::stol(value));
  } else {
    throw std::runtime_error("ROOT_TTree_Write_ContainerImp accepts some attributes, but not " + key);
  }
}

void ROOT_TBranch_Write_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  this->Storage_Associative_Write_Container::setFile(file);
  ROOT_TFileImp* root_tfile_imp = dynamic_cast<ROOT_TFileImp*>(file.get());
  if (root_tfile_imp == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Write_ContainerImp::setFile can't attach to non-ROOT file");
  }
  m_tfile = root_tfile_imp->getTFile();
  return;
}

void ROOT_TBranch_Write_ContainerImp::setParent(std::shared_ptr<IStorage_Write_Container> parent)
{
  this->Storage_Associative_Write_Container::setParent(parent);
  ROOT_TTree_Write_ContainerImp* root_ttree_imp = dynamic_cast<ROOT_TTree_Write_ContainerImp*>(parent.get());
  if (root_ttree_imp == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Write_ContainerImp::setParent");
  }
  m_tree = root_ttree_imp->getTTree();
  return;
}

void ROOT_TBranch_Write_ContainerImp::setupWrite(std::type_info const& type)
{
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Write_ContainerImp::setupWrite no tree found");
  }

  auto dictInfo = TDictionary::GetDictionary(type);
  if (m_branch == nullptr) {
    if (!dictInfo) {
      throw std::runtime_error("ROOT_TBranch_Write_ContainerImp::setupWrite unsupported type: " + DemangleName(type));
    }
    if (dictInfo->Property() & EProperty::kIsFundamental) {
      m_branch = m_tree->Branch(col_name().c_str(),
                                static_cast<void**>(nullptr),
                                (col_name() + typeNameToLeafList[dictInfo->GetName()]).c_str(),
                                4096);
    } else {
      m_branch =
        m_tree->Branch(col_name().c_str(), dictInfo->GetName(), static_cast<void**>(nullptr));
    }
  }
  if (m_branch == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Write_ContainerImp::setupWrite no branch created");
  }
  return;
}

void ROOT_TBranch_Write_ContainerImp::fill(void const* data)
{
  if (m_branch == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Write_ContainerImp::fill no branch found");
  }
  TLeaf* leaf = m_branch->GetLeaf(col_name().c_str());
  if (leaf != nullptr &&
      TDictionary::GetDictionary(leaf->GetTypeName())->Property() & EProperty::kIsFundamental) {
    m_branch->SetAddress(const_cast<void*>(data)); //FIXME: const_cast?
  } else {
    m_branch->SetAddress(&data);
  }
  m_branch->Fill();
  m_branch->ResetAddress();
  return;
}

void ROOT_TBranch_Write_ContainerImp::commit()
{
  // Forward the tree
  if (!m_tree) {
    throw std::runtime_error("ROOT_TBranch_Write_ContainerImp::commit no tree attached");
  }
  m_tree->SetEntries(m_branch->GetEntries());
  return;
}
