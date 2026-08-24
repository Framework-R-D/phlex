//A ROOT_RNTuple_Write_Container reads data products of a single type from vectors stored in an RNTuple field on disk.

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
      writer_->CommitDataset();
    }
  }

  void root_rntuple_write_container_imp::set_file(std::shared_ptr<i_storage_file> file)
  {
    storage_write_container::set_file(file);
    return;
  }

  std::uint64_t root_rntuple_write_container_imp::fill(void const* /*data*/)
  {
    throw std::runtime_error("root_rntuple_write_container_imp::fill not implemented");
  }

  void root_rntuple_write_container_imp::commit()
  {
    throw std::runtime_error("root_rntuple_write_container_imp::commit not implemented");
  }

  void root_rntuple_write_container_imp::setup_write(std::type_info const& /*type*/) { return; }
}
