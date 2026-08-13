// Copyright (C) 2025 ...

#include "storage/factories.hpp"

#include "storage/storage_file.hpp"
#include "storage/storage_read_container.hpp"
#include "storage/storage_write_association.hpp"
#include "storage/storage_write_container.hpp"

#ifdef USE_ROOT_STORAGE
#include "root_storage/root_tbranch_read_container.hpp"
#include "root_storage/root_tbranch_write_container.hpp"
#include "root_storage/root_tfile.hpp"
#include "root_storage/root_ttree_write_container.hpp"
#endif

#ifdef USE_RNTUPLE_STORAGE
#include "root_storage/root_rfield_read_container.hpp"
#include "root_storage/root_rfield_write_container.hpp"
#include "root_storage/root_rntuple_write_container.hpp"
#endif

#include <stdexcept>

namespace form::detail::experimental {

  using Major = form::technology::Major;

  std::shared_ptr<IStorage_File> createFile(form::technology::Id tech,
                                            std::string const& name,
                                            char mode)
  {
    switch (tech.major) {
    case Major::generic:
      // No technology specified: generic storage.
      return std::make_shared<Storage_File>(name, mode);
    case Major::root:
#ifdef USE_ROOT_STORAGE
      return std::make_shared<ROOT_TFileImp>(name, mode);
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case Major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

  std::shared_ptr<IStorage_Write_Container> createWriteAssociation(form::technology::Id tech,
                                                                   std::string const& name)
  {
    switch (tech.major) {
    case Major::generic:
      // No technology specified: generic storage.
      return std::make_shared<Storage_Write_Association>(name);
    case Major::root:
#ifdef USE_ROOT_STORAGE
      if (tech == form::technology::ROOT_TTREE) {
        return std::make_shared<ROOT_TTree_Write_ContainerImp>(name);
      } else if (tech == form::technology::ROOT_RNTUPLE) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<ROOT_RNTuple_Write_ContainerImp>(name);
#else
        throw std::runtime_error("FORM: ROOT RNTUPLE support is not compiled into this build");
#endif
      }
      // Recognized ROOT major, but an unsupported subtype/minor.
      throw std::runtime_error("FORM: requested ROOT write-association backend is not available");
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case Major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

  std::shared_ptr<IStorage_Read_Container> createReadContainer(form::technology::Id tech,
                                                               std::string const& name)
  {
    switch (tech.major) {
    case Major::generic:
      // No technology specified: generic storage.
      return std::make_shared<Storage_Read_Container>(name);
    case Major::root:
#ifdef USE_ROOT_STORAGE
      if (tech == form::technology::ROOT_TTREE) {
        return std::make_shared<ROOT_TBranch_Read_ContainerImp>(name);
      } else if (tech == form::technology::ROOT_RNTUPLE) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<ROOT_RField_Read_ContainerImp>(name);
#else
        throw std::runtime_error("FORM: ROOT RNTUPLE support is not compiled into this build");
#endif
      }
      // Recognized ROOT major, but an unsupported subtype/minor.
      throw std::runtime_error("FORM: requested ROOT read-container backend is not available");
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case Major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

  std::shared_ptr<IStorage_Write_Container> createWriteContainer(form::technology::Id tech,
                                                                 std::string const& name)
  {
    switch (tech.major) {
    case Major::generic:
      // No technology specified: generic storage.
      return std::make_shared<Storage_Write_Container>(name);
    case Major::root:
#ifdef USE_ROOT_STORAGE
      if (tech == form::technology::ROOT_TTREE) {
        return std::make_shared<ROOT_TBranch_Write_ContainerImp>(name);
      } else if (tech == form::technology::ROOT_RNTUPLE) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<ROOT_RField_Write_ContainerImp>(name);
#else
        throw std::runtime_error("FORM: ROOT RNTUPLE support is not compiled into this build");
#endif
      }
      // Recognized ROOT major, but an unsupported subtype/minor.
      throw std::runtime_error("FORM: requested ROOT write-container backend is not available");
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case Major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

} // namespace form::detail::experimental
