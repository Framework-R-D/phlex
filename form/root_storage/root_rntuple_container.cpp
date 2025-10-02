//A ROOT_RNTuple_Container reads data products of a single type from vectors stored in an RNTuple field on disk.

#include "root_rntuple_container.hpp"
#include "root_tfile.hpp"

#include "ROOT/RNTupleReader.hxx"
#include "ROOT/RNTupleView.hxx"
#include "ROOT/RNTupleWriter.hxx"
#include "TFile.h"

#include <exception>

namespace form::detail::experimental {
  ROOT_RNTuple_ContainerImp::ROOT_RNTuple_ContainerImp(std::string const& name) :
    Storage_Association(name), m_model(ROOT::RNTupleModel::Create())
  {
  }

  ROOT_RNTuple_ContainerImp::~ROOT_RNTuple_ContainerImp()
  {
    if (m_writer) {
      m_writer->CommitDataset();
    }
  }

  void ROOT_RNTuple_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
  {
    Storage_Container::setFile(file);
    return;
  }

  bool ROOT_RNTuple_ContainerImp::read(int /*id*/, void const** /*data*/, std::string& /*type*/)
  {
    throw std::runtime_error("ROOT_RNTuple_CotnainerImp::read not implemented");
  }

  void ROOT_RNTuple_ContainerImp::fill(void const* /*data*/)
  {
    throw std::runtime_error("ROOT_RNTuple_ContainerImp::fill not implemented");
  }

  void ROOT_RNTuple_ContainerImp::commit()
  {
    throw std::runtime_error("ROOT_RNTuple_ContainerImp::commit not implemented");
  }

  void ROOT_RNTuple_ContainerImp::setupWrite(std::string const& /*type*/) { return; }
}
