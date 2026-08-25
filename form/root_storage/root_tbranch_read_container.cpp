// Copyright (C) 2025 ...

#include "root_tbranch_read_container.hpp"
#include "demangle_name.hpp"
#include "root_tfile.hpp"

#include "TBranch.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TTree.h"

#include <gsl/pointers>

#include <mutex>
#include <unordered_map>

using namespace form::detail::experimental;

namespace {
  std::mutex& root_tbranch_read_mutex()
  {
    static std::mutex m;
    return m;
  }
}

root_tbranch_read_container_imp::root_tbranch_read_container_imp(std::string const& name) :
  storage_read_container(name)
{
}

void root_tbranch_read_container_imp::set_file(std::shared_ptr<i_storage_file> file)
{
  auto* root_file = dynamic_cast<root_tfile_imp*>(file.get());
  if (root_file == nullptr) {
    throw std::runtime_error(
      "root_tbranch_read_container_imp::set_file can't attach to non-ROOT file");
  }
  tfile_ = root_file->get_tfile();
}

void root_tbranch_read_container_imp::prime(std::type_info const& type)
{
  std::scoped_lock guard(root_tbranch_read_mutex());

  if (tfile_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::prime no file attached");
  }
  if (tree_ == nullptr) {
    tree_ = tfile_->Get<TTree>(top_name().c_str());
  }
  if (tree_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::prime no tree found with name " +
                             top_name());
  }
  if (branch_ == nullptr) {
    branch_ = tree_->GetBranch(col_name().c_str());
  }
  if (branch_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::prime no branch found");
  }

  auto* dict_info = TDictionary::GetDictionary(type);
  if (!dict_info) {
    throw std::runtime_error(
      std::string{"root_tbranch_read_container_imp::prime unsupported type: "} +
      demangle_name(type));
  }

  if (!(dict_info->Property() & EProperty::kIsFundamental)) {
    auto* klass = TClass::GetClass(type);
    if (!klass) {
      throw std::runtime_error(
        std::string{"root_tbranch_read_container_imp::prime missing TClass for type: "} +
        demangle_name(type));
    }
  }
}

bool root_tbranch_read_container_imp::read(int id, void const** data, std::type_info const& type)
{
  std::scoped_lock guard(root_tbranch_read_mutex());

  if (tfile_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::read no file attached");
  }
  if (tree_ == nullptr) {
    tree_ = tfile_->Get<TTree>(top_name().c_str());
  }
  if (tree_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::read no tree found with name " +
                             top_name());
  }
  if (branch_ == nullptr) {
    branch_ = tree_->GetBranch(col_name().c_str());
  }
  if (branch_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::read no branch found");
  }
  if (id >= tree_->GetEntries()) {
    return false;
  }

  gsl::owner<void*> branch_buffer = nullptr;
  auto* dict_info = TDictionary::GetDictionary(type);
  int branch_status = 0;

  if (!dict_info) {
    throw std::runtime_error(
      std::string{"root_tbranch_read_container_imp::read unsupported type: "} +
      demangle_name(type));
  }

  if (dict_info->Property() & EProperty::kIsFundamental) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto* fund_info = static_cast<TDataType*>(dict_info); // Already checked to be fundamental
    switch (fund_info->GetType()) {
    case kChar_t:
      branch_buffer = new Char_t;
      break;
    case kUChar_t:
      branch_buffer = new UChar_t;
      break;
    case kShort_t:
      branch_buffer = new Short_t;
      break;
    case kUShort_t:
      branch_buffer = new UShort_t;
      break;
    case kInt_t:
      branch_buffer = new Int_t;
      break;
    case kUInt_t:
      branch_buffer = new UInt_t;
      break;
    case kLong_t:
      branch_buffer = new Long_t;
      break;
    case kULong_t:
      branch_buffer = new ULong_t;
      break;
    case kLong64_t:
      branch_buffer = new Long64_t;
      break;
    case kULong64_t:
      branch_buffer = new ULong64_t;
      break;
    case kFloat_t:
      branch_buffer = new Float_t;
      break;
    case kDouble_t:
      branch_buffer = new Double_t;
      break;
    case kBool_t:
      branch_buffer = new Bool_t;
      break;
    default:
      throw std::runtime_error(
        std::string{"root_tbranch_read_container_imp::read unsupported fundamental type: "} +
        demangle_name(type));
    };
    branch_status = tree_->SetBranchAddress(
      col_name().c_str(), branch_buffer, nullptr, EDataType(fund_info->GetType()), false);
  } else {
    auto* klass = TClass::GetClass(type);
    if (!klass) {
      throw std::runtime_error(std::string{"root_tbranch_read_container_imp::read missing TClass"} +
                               " (col_name='" + col_name() + "', type='" + demangle_name(type) +
                               "')");
    }
    // ROOT returns ownership of dynamically created branch payload objects here.
    // NOLINTNEXTLINE(readability-redundant-casting)
    branch_buffer = gsl::owner<void*>{klass->New()};
    branch_status = tree_->SetBranchAddress(col_name().c_str(),
                                            reinterpret_cast<void*>(&branch_buffer),
                                            klass,
                                            EDataType::kOther_t,
                                            true);
  }

  if (branch_status < 0) {
    throw std::runtime_error(
      std::string{"root_tbranch_read_container_imp::read SetBranchAddress() failed"} +
      " (col_name='" + col_name() + "', type='" + demangle_name(type) + "')" + " with error code " +
      std::to_string(branch_status));
  }

  Long64_t tentry = tree_->LoadTree(id);
  branch_->GetEntry(tentry);
  *data = branch_buffer;

  // Reset the branch address to avoid unwanted ownership issues.
  branch_->ResetAddress();

  return true;
}

int root_tbranch_read_container_imp::entries()
{
  std::scoped_lock guard(root_tbranch_read_mutex());

  if (tfile_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::entries no file attached");
  }
  if (tree_ == nullptr) {
    tree_ = tfile_->Get<TTree>(top_name().c_str());
  }
  if (tree_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::entries no tree found with name " +
                             top_name());
  }
  if (branch_ == nullptr) {
    branch_ = tree_->GetBranch(col_name().c_str());
  }
  if (branch_ == nullptr) {
    throw std::runtime_error("root_tbranch_read_container_imp::entries no branch found");
  }
  return static_cast<int>(tree_->GetEntries());
}
