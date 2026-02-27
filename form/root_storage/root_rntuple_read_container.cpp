//A ROOT_RNTuple_Read_Container reads data products of a single type from vectors stored in an RNTuple field on disk.

#include "root_rntuple_read_container.hpp"
#include "root_tfile.hpp"

#include "ROOT/RNTupleReader.hxx"
#include "ROOT/RNTupleView.hxx"
#include "ROOT/RNTupleWriter.hxx"
#include "TFile.h"

#include <exception>

namespace form::detail::experimental {
  ROOT_RNTuple_Read_ContainerImp::ROOT_RNTuple_Read_ContainerImp(std::string const& name) :
    Storage_Read_Association(name)
  {
  }

  void ROOT_RNTuple_Read_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
  {
    Storage_Read_Container::setFile(file);
    return;
  }

  bool ROOT_RNTuple_Read_ContainerImp::read(int /*id*/, void const** /*data*/, std::type_info const& /*type*/)
  {
    throw std::runtime_error("ROOT_RNTuple_CotnainerImp::read not implemented");
  }
}
