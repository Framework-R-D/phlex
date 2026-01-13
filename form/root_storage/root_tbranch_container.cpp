// Copyright (C) 2025 ...

#include "root_tbranch_container.hpp"
#include "root_tfile.hpp"
#include "root_ttree_container.hpp"

#include "TBranch.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TTree.h"
#include "TClassEdit.h"

#include <unordered_map>

namespace {
  // Return the demangled type name
  std::string DemangleName(const std::type_info& type) {
    int errorCode;
    // The TClassEdit version works on both linux and Windows.
    char* demangledName = TClassEdit::DemangleTypeIdName(type, errorCode);
    if (errorCode != 0) {
      // NOTE: Instead of throwing, we could return the mangled name as a fallback.
      throw std::runtime_error("Failed to demangle type name");
    }
    std::string result(demangledName);
    std::free(demangledName);
    return result;
  }
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

ROOT_TBranch_ContainerImp::ROOT_TBranch_ContainerImp(std::string const& name) :
  Storage_Associative_Container(name), m_tfile(nullptr), m_tree(nullptr), m_branch(nullptr)
{
}

void ROOT_TBranch_ContainerImp::setAttribute(std::string const& key, std::string const& value)
{
  if (key == "auto_flush") {
    m_tree->SetAutoFlush(std::stol(value));
  } else {
    throw std::runtime_error("ROOT_TTree_ContainerImp accepts some attributes, but not " + key);
  }
}

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

void ROOT_TBranch_ContainerImp::setupWrite(std::type_info const& type)
{
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TBranch_ContainerImp::setupWrite no tree found");
  }

  auto dictInfo = TDictionary::GetDictionary(type);
  if (m_branch == nullptr) {
    if (!dictInfo) {
      throw std::runtime_error(std::string{"ROOT_TBranch_ContainerImp::setupWrite unsupported type: "} +
                               DemangleName(type));
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

void ROOT_TBranch_ContainerImp::fill(void const* data)
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

bool ROOT_TBranch_ContainerImp::read(int id, void const** data, std::type_info const& type)
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
  auto dictInfo = TDictionary::GetDictionary(type);
  int branchStatus = 0;
  if (!dictInfo) {
    throw std::runtime_error(std::string{"ROOT_TBranch_ContainerImp::read unsupported type: "} +
                             DemangleName(type));
  }

  if (dictInfo->Property() & EProperty::kIsFundamental) {
    auto fundInfo = static_cast<TDataType*>(dictInfo);
    branchBuffer = new char[fundInfo->Size()];
    branchStatus = m_tree->SetBranchAddress(
      col_name().c_str(), &branchBuffer, nullptr, EDataType(fundInfo->GetType()), true);
  } else {
    auto klass = TClass::GetClass(type);
    if (!klass) {
      throw std::runtime_error(
        std::string{"ROOT_TBranch_ContainerImp::read missing TClass"} +
        " (col_name='" + col_name() + "', type='" + DemangleName(type) + "')");
    }
    branchBuffer = klass->New();
    branchStatus =
      m_tree->SetBranchAddress(col_name().c_str(), &branchBuffer, klass, EDataType::kOther_t, true);
  }

  if (branchStatus < 0) {
    throw std::runtime_error(
      std::string{"ROOT_TBranch_ContainerImp::read SetBranchAddress() failed"} +
      " (col_name='" + col_name() + "', type='" + DemangleName(type) + "')" +
      " with error code " + std::to_string(branchStatus));
  }

  Long64_t tentry = m_tree->LoadTree(id);
  m_branch->GetEntry(tentry);
  *data = branchBuffer;
  return true;
}
