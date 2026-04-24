// Copyright (C) 2025 ...

#include "root_tbranch_read_container.hpp"
#include "demangle_name.hpp"
#include "root_tfile.hpp"

#include "TBranch.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TTree.h"

#include <unordered_map>

using namespace form::detail::experimental;

ROOT_TBranch_Read_ContainerImp::ROOT_TBranch_Read_ContainerImp(std::string const& name) :
  Storage_Read_Container(name), m_tfile(nullptr), m_tree(nullptr), m_branch(nullptr)
{
}

void ROOT_TBranch_Read_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  ROOT_TFileImp* root_tfile_imp = dynamic_cast<ROOT_TFileImp*>(file.get());
  if (root_tfile_imp == nullptr) {
    throw std::runtime_error(
      "ROOT_TBranch_Read_ContainerImp::setFile can't attach to non-ROOT file");
  }
  m_tfile = root_tfile_imp->getTFile();
  return;
}

bool ROOT_TBranch_Read_ContainerImp::read(int id, void const** data, std::type_info const& type)
{
  if (m_tfile == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Read_ContainerImp::read no file attached");
  }
  if (m_tree == nullptr) {
    m_tree = m_tfile->Get<TTree>(top_name().c_str());
  }
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Read_ContainerImp::read no tree found");
  }
  if (m_branch == nullptr) {
    m_branch = m_tree->GetBranch(col_name().c_str());
  }
  if (m_branch == nullptr) {
    throw std::runtime_error("ROOT_TBranch_Read_ContainerImp::read no branch found");
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
    //Assume this is a fundamental type like int or double
    auto fundInfo = static_cast<TDataType*>(TDictionary::GetDictionary(type));
    switch (fundInfo->GetType()) {
    case kChar_t:
      branchBuffer = new char;
      break;
    case kUChar_t:
      branchBuffer = new unsigned char;
      break;
    case kShort_t:
      branchBuffer = new short;
      break;
    case kUShort_t:
      branchBuffer = new unsigned short;
      break;
    case kInt_t:
      branchBuffer = new int;
      break;
    case kUInt_t:
      branchBuffer = new unsigned int;
      break;
    case kLong_t:
      branchBuffer = new long;
      break;
    case kULong_t:
      branchBuffer = new unsigned long;
      break;
    case kLong64_t:
      branchBuffer = new int64_t;
      break;
    case kULong64_t:
      branchBuffer = new uint64_t;
      break;
    case kFloat_t:
      branchBuffer = new float;
      break;
    case kDouble_t:
      branchBuffer = new double;
      break;
    case kBool_t:
      branchBuffer = new bool;
      break;
    default:
      throw std::runtime_error(
        std::string{"ROOT_TBranch_ContainerImp::read unsupported fundamental type: "} +
        DemangleName(type));
    };
    branchStatus = m_tree->SetBranchAddress(col_name().c_str(),
                                            reinterpret_cast<void*>(branchBuffer),
                                            nullptr,
                                            EDataType(fundInfo->GetType()),
                                            false);
  } else {
    auto klass = TClass::GetClass(type);
    if (!klass) {
      throw std::runtime_error(std::string{"ROOT_TBranch_ContainerImp::read missing TClass"} +
                               " (col_name='" + col_name() + "', type='" + DemangleName(type) +
                               "')");
    }
    branchBuffer = klass->New();
    branchStatus = m_tree->SetBranchAddress(
      col_name().c_str(), reinterpret_cast<void*>(&branchBuffer), klass, EDataType::kOther_t, true);
  }

  if (branchStatus < 0) {
    throw std::runtime_error(
      std::string{"ROOT_TBranch_ContainerImp::read SetBranchAddress() failed"} + " (col_name='" +
      col_name() + "', type='" + DemangleName(type) + "')" + " with error code " +
      std::to_string(branchStatus));
  }

  Long64_t tentry = m_tree->LoadTree(id);
  m_branch->GetEntry(tentry);
  *data = branchBuffer;

  // Reset the branch address to avoid unwanted ownership issues.
  m_branch->ResetAddress();

  return true;
}
