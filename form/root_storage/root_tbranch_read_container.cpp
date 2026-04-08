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
  // NOTE: incoming parameter `data` is `const` due to the constraints on how we
  // expect users to interact with the data; however, ROOT's SetBranchAddress
  // requires a non-const pointer, so we will need to cast away constness to call
  // it. We will ensure that we do not modify the data through this pointer, and
  // we will reset the branch address after reading to avoid any unintended
  // consequences of casting away the `const`ness.
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
    branchBuffer = new char[fundInfo->Size()];
    branchStatus = m_tree->SetBranchAddress(col_name().c_str(),
                                            reinterpret_cast<void*>(&branchBuffer),
                                            nullptr,
                                            EDataType(fundInfo->GetType()),
                                            true);
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
  auto br = m_tree->GetBranch(col_name().c_str());
  br->ResetAddress();

  return true;
}
