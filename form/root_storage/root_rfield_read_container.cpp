//A root_rfield_read_container reads data products of a single type from vectors stored in an RNTuple field on disk.

#include "root_rfield_read_container.hpp"
#include "demangle_name.hpp"
#include "root_tfile.hpp"

#include "ROOT/RNTupleReader.hxx"
#include "ROOT/RNTupleView.hxx"
#include "TDictionary.h"
#include "TFile.h"

#include <exception>
#include <mutex>

namespace {
  std::mutex& root_rfield_read_mutex()
  {
    static std::mutex m;
    return m;
  }
}

namespace form::detail::experimental {
  root_rfield_read_container_imp::root_rfield_read_container_imp(std::string const& name) :
    storage_read_container(name)
  {
  }

  root_rfield_read_container_imp::~root_rfield_read_container_imp() {}

  void root_rfield_read_container_imp::set_file(std::shared_ptr<i_storage_file> file)
  {
    storage_read_container::set_file(file);

    auto form_root_file = dynamic_cast<root_tfile_imp*>(file.get());
    if (form_root_file) {
      tfile_ = form_root_file->get_tfile();
    } else {
      throw std::runtime_error("root_rfield_read_container_imp::set_file failed to convert an "
                               "i_storage_file to a root_tfile_imp.  "
                               "root_rfield_read_container_imp only works with TFiles.");
    }

    if (!tfile_) {
      throw std::runtime_error(
        "root_rfield_read_container_imp::set_file failed to get a TFile from a root_tfile_imp");
    }

    return;
  }

  void root_rfield_read_container_imp::prime(std::type_info const& type)
  {
    std::lock_guard<std::mutex> guard(root_rfield_read_mutex());

    if (!tfile_) {
      throw std::runtime_error("root_rfield_read_container_imp::prime No file loaded");
    }

    if (!reader_) {
      reader_ = ROOT::RNTupleReader::Open(top_name(), tfile_->GetName());
    }

    if (!view_) {
      create_view(type);
    }

    if (!TDictionary::GetDictionary(type)) {
      throw std::runtime_error("root_rfield_read_container_imp::prime unsupported type");
    }
  }

  bool root_rfield_read_container_imp::read(int id, void const** data, std::type_info const& type)
  {
    std::lock_guard<std::mutex> guard(root_rfield_read_mutex());

    //Connect to file at the last possible moment at the cost of a little run-time branching
    if (!view_) {
      create_view(type);
    }

    if (id >= static_cast<int>(reader_->GetNEntries())) {
      return false;
    }

    //Using RNTupleView<> to read instead of reusing REntry gives us full schema evolution support: the ROOT feature that lets us read files with an old class version into a new class version's memory.
    auto buffer = view_->GetField().CreateObject<void>(); //PHLEX gets ownership of this memory
    assert(buffer);

    view_->BindRawPtr(buffer.get());
    try {
      (*view_)(id);
    } catch (ROOT::RException const& e) {
      throw std::runtime_error("root_rfield_read_container_imp::read got a ROOT exception: " +
                               std::string(e.what()));
    }
    *data =
      buffer.release(); //Ownership transferred to Phlex through Persistence and interface layers.
    //Any framework using FORM must free this memory.  FORM holds no reference to it.

    return true;
  }

  int root_rfield_read_container_imp::entries()
  {
    std::lock_guard<std::mutex> guard(root_rfield_read_mutex());

    if (!reader_) {
      if (!tfile_) {
        throw std::runtime_error("root_rfield_read_container_imp::entries No file loaded");
      }
      reader_ = ROOT::RNTupleReader::Open(top_name(), tfile_->GetName());
    }

    if (!view_ &&
        (reader_->GetDescriptor().FindFieldId(col_name()) == ROOT::kInvalidDescriptorId)) {
      throw std::runtime_error("root_rfield_read_container_imp::entries field " + col_name() +
                               " does not exist");
    }

    return static_cast<int>(reader_->GetNEntries());
  }

  void root_rfield_read_container_imp::create_view(std::type_info const& type)
  {
    if (!reader_) { //First time this RNTuple is read this job
      if (!tfile_) {
        throw std::runtime_error(
          "root_rfield_read_container_imp::create_view No file loaded to read "
          "from on first read() call!");
      }

      reader_ = ROOT::RNTupleReader::Open(top_name(), tfile_->GetName());
    }

    try {
      view_ =
        std::make_unique<ROOT::RNTupleView<void>>(reader_->GetView(col_name(), nullptr, type));
    } catch (ROOT::RException const& e) {
      //RNTupleView<void> will fail to create a field for fields written in streamer mode or for which type does not match the field's type on disk.  Passing an empty string for type forces it to create the same type of field as the object on disk.  Do this to handle streamer fields, then perform our own type check.
      view_ = std::make_unique<ROOT::RNTupleView<void>>(reader_->GetView(col_name(), nullptr, ""));
      //TClass takes the "std::" off of "std::vector<>" when RNTuple's on-disk format doesn't.  Convert RNTuple's type name to match TClass for manual type check because our dictionary of choice will likely be the same as TClass.
      if (!TDictionary::GetDictionary(type) ||
          !TDictionary::GetDictionary(view_->GetField().GetTypeName().c_str()) ||
          (strcmp(TDictionary::GetDictionary(view_->GetField().GetTypeName().c_str())->GetName(),
                  TDictionary::GetDictionary(type)->GetName()) != 0)) {
        throw std::runtime_error(
          "root_rfield_read_container_imp::create_view type " + demangle_name(type) +
          " requested for a field named " + col_name() +
          " does not match the type in the file: " + view_->GetField().GetTypeName());
      }
    }
  }
}
