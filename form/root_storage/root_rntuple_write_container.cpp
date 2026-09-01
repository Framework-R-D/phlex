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
    storage_write_association(name), model(ROOT::RNTupleModel::Create())
  {
  }

  root_rntuple_write_container_imp::~root_rntuple_write_container_imp()
  {
    if (writer) {
      writer->CommitDataset();
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
