// Copyright (C) 2025 ...

#include "root_ttree_container.hpp"
#include "root_tfile.hpp"

#include "TBranch.h"
#include "TFile.h"
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

ROOT_TTree_ContainerImp::ROOT_TTree_ContainerImp(const std::string& name) :
  m_name(name),
  m_treeName(""),
  m_branchName(""),
  m_file(nullptr),
  m_tree(nullptr),
  m_ownsTree(false),
  m_branch(nullptr)
{
  auto del_pos = m_name.find("/");
  if (del_pos != std::string::npos) {
    m_treeName = m_name.substr(0, del_pos);
    m_branchName = m_name.substr(del_pos + 1);
  } else {
    m_treeName = m_name;
    m_branchName = m_name;
  }
}

ROOT_TTree_ContainerImp::~ROOT_TTree_ContainerImp()
{
  if (m_ownsTree && m_tree != nullptr) {
    m_tree->Write();
    delete m_tree;
  }
}

const std::string& ROOT_TTree_ContainerImp::name() { return m_name; }

void ROOT_TTree_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  m_file = dynamic_cast<ROOT_TFileImp*>(file.get())->getTFile();
  return;
}

void ROOT_TTree_ContainerImp::fill(const void* data, const std::string& type)
{
  if (m_file == nullptr) {
    throw std::string("ROOT_TTree_ContainerImp::fill no file attached");
  }
  if (m_tree == nullptr) {
    m_tree = m_file->Get<TTree>(m_treeName.c_str());
  }
  if (m_tree == nullptr) {
    m_tree = new TTree(m_treeName.c_str(), m_treeName.c_str());
    m_tree->SetDirectory(m_file.get());
    m_ownsTree = true;
  }
  if (m_tree == nullptr) {
    throw std::string("ROOT_TTree_ContainerImp::fill no tree created");
  }
  if (m_branch == nullptr) {
    m_branch = m_tree->GetBranch(m_branchName.c_str());
  }

  auto dictInfo = TDictionary::GetDictionary(type.c_str());
  if (m_branch == nullptr) {
    if (!dictInfo) {
      throw std::string("Unsupported type passed to ROOT_TTree_ContainerImp::fill: " + type);
    }
    if (dictInfo->Property() & EProperty::kIsFundamental) {
      m_branch = m_tree->Branch(m_branchName.c_str(),
                                &data,
                                (m_branchName + typeNameToLeafList[dictInfo->GetName()]).c_str(),
                                4096);
    } else {
      m_branch = m_tree->Branch(m_branchName.c_str(), dictInfo->GetName(), &data);
    }
  }
  if (m_branch == nullptr) {
    throw std::string("ROOT_TTree_ContainerImp::fill no branch created");
  }
  if (dictInfo->Property() & EProperty::kIsFundamental) {
    m_branch->SetAddress(const_cast<void*>(data)); //FIXME: const_cast?
  } else {
    m_branch->SetAddress(&data);
  }
  m_branch->Fill();
  m_branch->ResetAddress();
  // Forward the tree
  m_tree->SetEntries(m_branch->GetEntries());
  return;
}

bool ROOT_TTree_ContainerImp::read(int id, const void** data, std::string& type)
{
  if (m_file == nullptr) {
    throw std::string("ROOT_TTree_ContainerImp::read no file attached");
  }
  if (m_tree == nullptr) {
    m_tree = m_file->Get<TTree>(m_treeName.c_str());
  }
  if (m_tree == nullptr) {
    throw std::string("ROOT_TTree_ContainerImp::read no tree found");
  }
  if (m_branch == nullptr) {
    m_branch = m_tree->GetBranch(m_branchName.c_str());
  }
  if (m_branch == nullptr) {
    throw std::string("ROOT_TTree_ContainerImp::read no branch found");
  }
  if (id > m_tree->GetEntries())
    return false;

  void* branchBuffer = nullptr;
  auto dictInfo = TClass::GetClass(type.c_str());
  if (dictInfo) {
    branchBuffer = dictInfo->New();
    m_tree->SetBranchAddress(m_branchName.c_str(),
                             &branchBuffer,
                             TClass::GetClass(type.c_str()),
                             EDataType::kOther_t,
                             true);
  } else {
    //Assume this is a fundamental type like int or double
    auto fundInfo = static_cast<TDataType*>(TDictionary::GetDictionary(type.c_str()));
    branchBuffer = new char[fundInfo->Size()];
    m_tree->SetBranchAddress(
      m_branchName.c_str(), &branchBuffer, nullptr, EDataType(fundInfo->GetType()), true);
  }
  Long64_t tentry = m_tree->LoadTree(id);
  m_branch->GetEntry(tentry);
  *data = branchBuffer;
  return true;
}
