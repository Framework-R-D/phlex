// Copyright (C) 2025 ...

#include "root_tbranch_container.hpp"
#include "root_tfile.hpp"
#include "root_ttree_container.hpp"

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

ROOT_TBranch_ContainerImp::ROOT_TBranch_ContainerImp(const std::string& name) :
  Storage_Associative_Container(name), m_tfile(nullptr), m_tree(nullptr), m_branch(nullptr)
{
}

ROOT_TBranch_ContainerImp::~ROOT_TBranch_ContainerImp() {}

void ROOT_TBranch_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  this->Storage_Associative_Container::setFile(file);
  ROOT_TFileImp* root_tfile_imp = dynamic_cast<ROOT_TFileImp*>(file.get());
  if (root_tfile_imp == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::setFile can't attach to non-ROOT file");
  }
  m_tfile = root_tfile_imp->getTFile();
  return;
}

void ROOT_TBranch_ContainerImp::setParent(std::shared_ptr<IStorage_Container> parent)
{
  this->Storage_Associative_Container::setParent(parent);
  ROOT_TTree_ContainerImp* root_ttree_imp = dynamic_cast<ROOT_TTree_ContainerImp*>(parent.get());
  if (root_ttree_imp == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::setParent");
  }
  m_tree = root_ttree_imp->getTTree();
  return;
}

void ROOT_TBranch_ContainerImp::setupWrite(const std::string& type)
{
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::setupWrite no tree found");
  }

  auto dictInfo = TDictionary::GetDictionary(type.c_str());
  if (m_branch == nullptr) {
    if (!dictInfo) {
      throw std::runtime_error("ROOT_TBranch_ContainerImp::setupWrite unsupported type: " + type);
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
    throw std::runtime_error("ROOT_TBranch_ContainerImp::setupWrite no branch created");
  }
  return;
}

void ROOT_TBranch_ContainerImp::fill(const void* data)
{
  if (m_branch == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::fill no branch found");
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

void ROOT_TBranch_ContainerImp::commit()
{
  // Forward the tree
  if (!m_tree) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::commit no tree attached");
  }
  m_tree->SetEntries(m_branch->GetEntries());
  return;
}

bool ROOT_TBranch_ContainerImp::read(int id, const void** data, std::string& type)
{
  if (m_tfile == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::read no file attached");
  }
  if (m_tree == nullptr) {
    m_tree = m_tfile->Get<TTree>(top_name().c_str());
  }
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::read no tree found");
  }
  if (m_branch == nullptr) {
    m_branch = m_tree->GetBranch(col_name().c_str());
  }
  if (m_branch == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::read no branch found");
  }
  if (id > m_tree->GetEntries())
    return false;

  void* branchBuffer = nullptr;
  auto dictInfo = TClass::GetClass(type.c_str());
  int branchStatus = 0;
  if (dictInfo) {
    branchBuffer = dictInfo->New();
    branchStatus = m_tree->SetBranchAddress(
      col_name().c_str(), &branchBuffer, TClass::GetClass(type.c_str()), EDataType::kOther_t, true);
  } else {
    //Assume this is a fundamental type like int or double
    auto fundInfo = static_cast<TDataType*>(TDictionary::GetDictionary(type.c_str()));
    branchBuffer = new char[fundInfo->Size()];
    branchStatus = m_tree->SetBranchAddress(
      col_name().c_str(), &branchBuffer, nullptr, EDataType(fundInfo->GetType()), true);
  }

  if (branchStatus < 0) {
    throw std::runtime_error(
      "ROOT_TBranch_ContainerImp::read SetBranchAddress() failed with error code " +
      std::to_string(branchStatus));
  }

  Long64_t tentry = m_tree->LoadTree(id);
  m_branch->GetEntry(tentry);
  *data = branchBuffer;
  return true;
}
