// Copyright (C) 2025 ...

#include "root_ttree_write_container.hpp"
#include "root_tfile.hpp"

#include "TFile.h"
#include "TTree.h"

#include <gsl/pointers>

using namespace form::detail::experimental;

root_ttree_write_container_imp::root_ttree_write_container_imp(std::string const& name) :
  storage_write_association(name)
{
}

void root_ttree_write_container_imp::set_file(std::shared_ptr<i_storage_file> file)
{
  this->storage_write_association::set_file(file);
  auto* root_file = dynamic_cast<root_tfile_imp*>(file.get());
  if (root_file == nullptr) {
    throw std::runtime_error(
      "root_ttree_write_container_imp::set_file can't attach to non-ROOT file");
  }
  tfile_ = root_file->get_tfile();
}

void root_ttree_write_container_imp::setup_write(std::type_info const& /* type*/)
{
  if (tfile_ == nullptr) {
    throw std::runtime_error("root_ttree_write_container_imp::setup_write no file attached");
  }
  if (tree_ == nullptr) {
    tree_.reset(tfile_->Get<TTree>(name().c_str()));
  }
  if (tree_ == nullptr) {
    // Mark the raw allocation as an owning pointer before transferring it.
    // NOLINTNEXTLINE(readability-redundant-casting)
    tree_.reset(gsl::owner<TTree*>{new TTree(name().c_str(), name().c_str())});
    tree_->SetDirectory(tfile_.get());
  }
  if (tree_ == nullptr) {
    throw std::runtime_error("root_ttree_write_container_imp::setup_write no tree created");
  }
}

std::uint64_t root_ttree_write_container_imp::fill(void const* /* data*/)
{
  throw std::runtime_error("root_ttree_write_container_imp::fill not implemented");
}

void root_ttree_write_container_imp::commit()
{
  throw std::runtime_error("root_ttree_write_container_imp::commit not implemented");
}

TTree* root_ttree_write_container_imp::get_ttree() { return tree_.get(); }

void root_ttree_write_container_imp::ttree_deleter::operator()(gsl::owner<TTree*> t) const
{
  if (t) {
    t->GetDirectory()->WriteTObject(t);
    delete t;
  }
}
