//A root_rntuple_write_container_imp is a storage_write_association that coordinates the RNTuple-specific, file-based resources (writer, model, entry) shared by several root_rfield_write_container_imps; it does not itself write a data product (see root_rfield_write_container_imp for the per-field write path).

#include "root_rntuple_write_container.hpp"
#include "root_tfile.hpp"

#include "ROOT/RNTupleReader.hxx"
#include "ROOT/RNTupleView.hxx"
#include "ROOT/RNTupleWriter.hxx"
#include "TFile.h"

#include <exception>

namespace form::detail::experimental {
  root_rntuple_write_container_imp::root_rntuple_write_container_imp(std::string const& name) :
    storage_write_association(name), model_(ROOT::RNTupleModel::Create())
  {
  }

  root_rntuple_write_container_imp::~root_rntuple_write_container_imp()
  {
    if (writer_) {
      try {
        writer_->CommitDataset();
      } catch (ROOT::RException const& e) {
        std::cerr << "Failed to commit RNTuple " << name() << " at destruction.\n";
      }
    }
  }

  void root_rntuple_write_container_imp::set_file(std::shared_ptr<i_storage_file> file)
  {
    storage_write_container::set_file(file);

    auto form_root_file = dynamic_pointer_cast<root_tfile_imp>(file);
    if (form_root_file) {
      tfile_ = form_root_file->get_tfile();
    } else {
      throw std::runtime_error("root_rntuple_write_container_imp::set_file failed to convert an "
                               "i_storage_file to a root_tfile_imp.  "
                               "root_rntuple_write_container_imp only works with TFiles.");
    }
  }

  std::uint64_t root_rntuple_write_container_imp::fill(void const* /*data*/)
  {
    throw std::runtime_error("root_rntuple_write_container_imp::fill not implemented");
  }

  void root_rntuple_write_container_imp::commit()
  {
    throw std::runtime_error("root_rntuple_write_container_imp::commit not implemented");
  }

  ROOT::RNTupleWriter& root_rntuple_write_container_imp::get_writer()
  {
    if (!writer_) {
      if (!tfile_)
        throw std::runtime_error("root_rntuple_write_container_imp::setup_write no file loaded to "
                                 "write to on first fill() call");
      writer_ = ROOT::RNTupleWriter::Append(std::move(model_), name(), *tfile_);
    }

    return *writer_;
  }

  std::unique_ptr<ROOT::RNTupleModel> const& root_rntuple_write_container_imp::get_model() const
  {
    return model_;
  }

  RRawPtrWriteEntry& root_rntuple_write_container_imp::get_entry()
  {
    if (!entry_) {
      entry_ = get_writer().CreateRawPtrWriteEntry();
    }
    return *entry_;
  }

  void root_rntuple_write_container_imp::setup_write(std::type_info const& /*type*/) {}
}
