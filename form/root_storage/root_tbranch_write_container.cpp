// Copyright (C) 2025 ...

#include "root_tbranch_write_container.hpp"
#include "demangle_name.hpp"
#include "root_tfile.hpp"
#include "root_ttree_write_container.hpp"

#include "TBranch.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TTree.h"

#include <unordered_map>

using namespace form::detail::experimental;

root_tbranch_write_container_imp::root_tbranch_write_container_imp(std::string const& name) :
  storage_associative_write_container(name)
{
}

void root_tbranch_write_container_imp::set_attribute(std::string const& key,
                                                     std::string const& value)
{
  if (key == "auto_flush") {
    tree_->SetAutoFlush(std::stol(value));
  } else {
    throw std::runtime_error("root_ttree_write_container_imp accepts some attributes, but not " +
                             key);
  }
}

void root_tbranch_write_container_imp::set_file(std::shared_ptr<i_storage_file> file)
{
  this->storage_associative_write_container::set_file(file);
  auto* root_file = dynamic_cast<root_tfile_imp*>(file.get());
  if (root_file == nullptr) {
    throw std::runtime_error(
      "root_tbranch_write_container_imp::set_file can't attach to non-ROOT file");
  }
  tfile_ = root_file->get_tfile();
}

void root_tbranch_write_container_imp::set_parent(std::shared_ptr<i_storage_write_container> parent)
{
  this->storage_associative_write_container::set_parent(parent);
  auto* root_ttree_imp = dynamic_cast<root_ttree_write_container_imp*>(parent.get());
  if (root_ttree_imp == nullptr) {
    throw std::runtime_error("root_tbranch_write_container_imp::set_parent");
  }
  tree_ = root_ttree_imp->get_ttree();
}

void root_tbranch_write_container_imp::setup_write(std::type_info const& type)
{
  //Type name conversion based on https://root.cern.ch/doc/master/classTTree.html#ac1fa9466ce018d4aa739b357f981c615
  //An empty leaf list (i.e. for a type not in this map) defaults to Float_t; this is intentional.
  static std::map<Int_t, std::string> type_name_to_leaf_list = {{kChar_t, "/B"},
                                                                {kUChar_t, "/b"},
                                                                {kInt_t, "/I"},
                                                                {kUInt_t, "/i"},
                                                                {kFloat_t, "/F"},
                                                                {kDouble_t, "/D"},
                                                                {kShort_t, "/S"},
                                                                {kUShort_t, "/s"},
                                                                {kLong_t, "/G"},
                                                                {kULong_t, "/g"},
                                                                {kLong64_t, "/L"},
                                                                {kULong64_t, "/l"},
                                                                {kBool_t, "/O"}};

  if (tree_ == nullptr) {
    throw std::runtime_error("root_tbranch_write_container_imp::setup_write no tree found");
  }

  auto* dict_info = TDictionary::GetDictionary(type);
  if (branch_ == nullptr) {
    if (!dict_info) {
      throw std::runtime_error("root_tbranch_write_container_imp::setup_write unsupported type: " +
                               demangle_name(type));
    }
    if (dict_info->Property() & EProperty::kIsFundamental) {
      branch_ = tree_->Branch(
        col_name().c_str(),
        static_cast<void*>(nullptr), // Overload selection
        //NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        (col_name() + type_name_to_leaf_list[static_cast<TDataType*>(dict_info)->GetType()])
          .c_str(),
        4096);
    } else {
      branch_ = tree_->Branch(col_name().c_str(), dict_info->GetName(), nullptr);
    }
  }
  if (branch_ == nullptr) {
    throw std::runtime_error("root_tbranch_write_container_imp::setup_write no branch created");
  }
}

std::uint64_t root_tbranch_write_container_imp::fill(void const* data)
{
  // NOTE: incoming parameter `data` is `const` due to the constraints on how we
  // expect users to interact with the data; however, ROOT's SetBranchAddress
  // requires a non-const pointer, so we will need to cast away constness to call
  // it. We will ensure that we do not modify the data through this pointer, and
  // we will reset the branch address after reading to avoid any unintended
  // consequences of casting away the `const`ness.
  if (branch_ == nullptr) {
    throw std::runtime_error("root_tbranch_write_container_imp::fill no branch found");
  }
  TLeaf* leaf = branch_->GetLeaf(col_name().c_str());
  if (leaf != nullptr &&
      TDictionary::GetDictionary(leaf->GetTypeName())->Property() & EProperty::kIsFundamental) {
    branch_->SetAddress(const_cast<void*>(data));
  } else {
    branch_->SetAddress(reinterpret_cast<void*>(&data));
  }
  // TBranch::Fill() returns the number of bytes committed, or a negative value on a write error.
  // ROOT increments entry count before a basket write can fail, so check return value first
  Int_t const nbytes = branch_->Fill();
  branch_->ResetAddress();
  if (nbytes < 0) {
    throw std::runtime_error("root_tbranch_write_container_imp::fill TBranch::Fill() failed for " +
                             col_name());
  }

  // 0-based entries: GetEntries() is the total count after this Fill(); row = count - 1.
  // GetEntries() >= 1 here (Fill() succeeded), so the row is non-negative.
  return static_cast<std::uint64_t>(branch_->GetEntries() - 1);
}

void root_tbranch_write_container_imp::commit()
{
  // Forward the tree
  if (!tree_) {
    throw std::runtime_error("root_tbranch_write_container_imp::commit no tree attached");
  }

  if (!branch_) {
    throw std::runtime_error("root_tbranch_write_container_imp::commit no branch found");
  }

  if (branch_->GetEntries() == tree_->GetEntries()) {
    throw std::runtime_error(
      "root_tbranch_write_container_imp::commit called without new entries since last fill/commit");
  }

  tree_->SetEntries(branch_->GetEntries());
}
