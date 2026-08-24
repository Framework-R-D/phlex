//A ROOT_RField_Write_Container writes data products of a single type from vectors stored in an RNTuple field on disk.

#include "root_rfield_write_container.hpp"
#include "demangle_name.hpp"
#include "root_rntuple_write_container.hpp"
#include "root_tfile.hpp"

#include "ROOT/RNTupleWriter.hxx"
#include "TFile.h"

#include <exception>
#include <iostream>

namespace form::detail::experimental {
  root_rfield_write_container_imp::root_rfield_write_container_imp(std::string const& name) :
    storage_associative_write_container(name)
  {
  }

  void root_rfield_write_container_imp::set_attribute(std::string const& key,
                                                      std::string const& value)
  {
    if (key == "force_streamer_field" && value == "true") {
      force_streamer_field_ = true;
    } else {
      throw std::runtime_error(
        "root_rfield_write_container_imp supports some attributes, but not " + key);
    }
  }

  void root_rfield_write_container_imp::set_file(std::shared_ptr<i_storage_file> file)
  {
    storage_write_container::set_file(file);

    auto form_root_file = dynamic_pointer_cast<root_tfile_imp>(file);
    if (form_root_file) {
      tfile_ = form_root_file->get_tfile();
    } else {
      throw std::runtime_error("root_rfield_write_container_imp::set_file failed to convert an "
                               "i_storage_file to a root_tfile_imp.  "
                               "root_rfield_write_container_imp only works with TFiles.");
    }

    if (!tfile_) {
      throw std::runtime_error(
        "root_rfield_write_container_imp::set_file failed to get a TFile from a root_tfile_imp");
    }

    return;
  }

  void root_rfield_write_container_imp::set_parent(
    std::shared_ptr<i_storage_write_container> parent)
  {
    this->storage_associative_write_container::set_parent(parent);
    auto parent_derived = dynamic_pointer_cast<root_rntuple_write_container_imp>(parent);
    if (!parent_derived) {
      throw std::runtime_error("root_rfield_write_container_imp::set_parent parent is not a "
                               "root_rntuple_write_container_imp!  Something "
                               "may be wrong with how Storage works.");
    }
    rntuple_parent_ = parent_derived;
  }

  std::uint64_t root_rfield_write_container_imp::fill(void const* data)
  {
    if (!rntuple_parent_) {
      throw std::runtime_error(
        "root_rfield_write_container_imp::fill No parent RNTuple set up before first fill() call");
    }

    if (!rntuple_parent_->writer_) {
      if (!tfile_) {
        throw std::runtime_error(
          "root_rfield_write_container_imp::fill No file loaded to write to on first fill() call");
      }

      rntuple_parent_->writer_ =
        ROOT::RNTupleWriter::Append(std::move(rntuple_parent_->model_), top_name(), *tfile_);
      rntuple_parent_->entry_ = rntuple_parent_->writer_->CreateRawPtrWriteEntry();
    }
    rntuple_parent_->entry_->BindRawPtr(col_name(), data);

    // Unlike a TBranch, an RNTuple entry is only written on commit();
    // every field bound before that commit shares one entry.
    // Return the 0-based index that pending entry will occupy (the current entry count).
    return static_cast<std::uint64_t>(rntuple_parent_->writer_->GetNEntries());
  }

  void root_rfield_write_container_imp::commit()
  {
    if (!rntuple_parent_) {
      throw std::runtime_error("root_rfield_write_container_imp::commit No parent RNTuple set up.  "
                               "You may have called commit() without calling set_parent() first.");
    }

    if (!rntuple_parent_->entry_) {
      throw std::runtime_error(
        "root_rfield_write_container_imp::commit No RRawPtrWriteEntry set up.  "
        "You may have called commit() without calling fill() first.");
    }
    assert(rntuple_parent_->writer_); //writer_ and entry_ are set in the same place: fill()
    rntuple_parent_->writer_->Fill(*rntuple_parent_->entry_);
  }

  //setup_write() may not be called after the first time fill() is called.
  //If needed in the future, this can be changed by using RNTupleModels' updater facilities.
  void root_rfield_write_container_imp::setup_write(std::type_info const& type)
  {
    if (!rntuple_parent_) {
      throw std::runtime_error(
        "root_rfield_write_container_imp::setup_write No parent RNTuple set up.  "
        "You may have called setup_write() before set_parent().");
    }

    auto const& type_name = demangle_name(type);
    std::unique_ptr<ROOT::RFieldBase> field;

    if (force_streamer_field_) {
      field = std::make_unique<ROOT::RStreamerField>(col_name(), type_name);
    } else {
      auto field_result = ROOT::RFieldBase::Create(col_name(), type_name);
      if (field_result) {
        field = field_result.Unwrap();
      } else {
        std::cerr
          << "root_rfield_write_container_imp::setup_write could not create column-wise storage "
             "for "
          << type_name
          << ".  This class is probably using something obsolete like TLorentzVector.  Storing it "
             "in streamer mode to keep the application going."
          << std::endl;
        field = std::make_unique<ROOT::RStreamerField>(col_name(), type_name);
      }
    }

    rntuple_parent_->model_->AddField(std::move(field));
  }

}
