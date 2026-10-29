// Copyright (C) 2025 ...

#include "root_tbranch_read_container.hpp"

#include "demangle_name.hpp"
#include "root_tfile.hpp"
#include "storage/istorage.hpp"
#include "storage/storage_read_container.hpp"

#include <RtypesCore.h>
#include <TBranch.h>
#include <TClass.h>
#include <TDataType.h>
#include <TDictionary.h>
// Required for TFile member access; include-cleaner does not associate those uses with TFile.h.
#include <TFile.h> // IWYU pragma: keep
#include <TTree.h>
#include <gsl/pointers>

#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

using namespace form::detail::experimental;

namespace {
  std::mutex& root_tbranch_read_mutex()
  {
    static std::mutex m;
    return m;
  }

  // Allocates a buffer for a fundamental branch type; the caller owns it.
  gsl::owner<void*> new_fundamental(Int_t data_type, std::type_info const& type)
  {
    switch (data_type) {
    case kChar_t:
      return new Char_t;
    case kUChar_t:
      return new UChar_t;
    case kShort_t:
      return new Short_t;
    case kUShort_t:
      return new UShort_t;
    case kInt_t:
      return new Int_t;
    case kUInt_t:
      return new UInt_t;
    case kLong_t:
      return new Long_t;
    case kULong_t:
      return new ULong_t;
    case kLong64_t:
      return new Long64_t;
    case kULong64_t:
      return new ULong64_t;
    case kFloat_t:
      return new Float_t;
    case kDouble_t:
      return new Double_t;
    case kBool_t:
      return new Bool_t;
    default:
      throw std::runtime_error(
        std::string{"root_tbranch_read_container_imp::read unsupported fundamental type: "} +
        demangle_name(type));
    }
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
  std::scoped_lock const guard(root_tbranch_read_mutex());

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

bool root_tbranch_read_container_imp::read(std::uint64_t id,
                                           void const** data,
                                           std::type_info const& type)
{
  std::scoped_lock const guard(root_tbranch_read_mutex());

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
  if (std::cmp_greater_equal(id, tree_->GetEntries())) {
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
    branch_buffer = new_fundamental(fund_info->GetType(), type);
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

  // ROOT addresses entries as Long64_t; id < GetEntries() was checked above.
  branch_->GetEntry(tree_->LoadTree(static_cast<Long64_t>(id)));
  *data = branch_buffer;

  // Reset the branch address to avoid unwanted ownership issues.
  branch_->ResetAddress();

  return true;
}

std::uint64_t root_tbranch_read_container_imp::entries()
{
  std::scoped_lock const guard(root_tbranch_read_mutex());

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
  return static_cast<std::uint64_t>(tree_->GetEntries());
}
