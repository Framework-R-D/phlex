// Copyright (C) 2025 .....

#ifndef FORM_UTIL_FACTORIES_HPP
#define FORM_UTIL_FACTORIES_HPP

#include "form/technology.hpp"

#include "storage/istorage.hpp"
#include "storage/storage_read_association.hpp"
#include "storage/storage_write_association.hpp"
#include "storage/storage_read_container.hpp"
#include "storage/storage_write_container.hpp"
#include "storage/storage_file.hpp"

#ifdef USE_ROOT_STORAGE
#include "root_storage/root_tbranch_read_container.hpp"
#include "root_storage/root_tbranch_write_container.hpp"
#include "root_storage/root_tfile.hpp"
#include "root_storage/root_ttree_read_container.hpp"
#include "root_storage/root_ttree_write_container.hpp"
#endif

#include <memory>
#include <string>

namespace form::detail::experimental {

  inline std::shared_ptr<IStorage_File> createFile(int tech, std::string const& name, char mode)
  {
    if (form::technology::GetMajor(tech) == form::technology::ROOT_MAJOR) {
#ifdef USE_ROOT_STORAGE
      return std::make_shared<ROOT_TFileImp>(name, mode);
#endif
    } else if (form::technology::GetMajor(tech) == form::technology::HDF5_MAJOR) {
      // Handle HDF5 file creation when implemented
      // return std::make_shared<HDF5_FileImp>(name, mode);
    }
    return std::make_shared<Storage_File>(name, mode);
  }

  inline std::shared_ptr<IStorage_Read_Container> createReadAssociation(int tech, std::string const& name)
  {
    if (form::technology::GetMajor(tech) == form::technology::ROOT_MAJOR) {
      if (form::technology::GetMinor(tech) == form::technology::ROOT_TTREE_MINOR) {
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TTree_Read_ContainerImp>(name);
#endif // USE_ROOT_STORAGE
      } else if (form::technology::GetMinor(tech) == form::technology::ROOT_RNTUPLE_MINOR) {
#ifdef USE_RNTUPLE_STORAGE
        //return std::make_shared<ROOT_RNTuple_Read_ContainerImp>(name);
#endif // USE_RNTUPLE_STORAGE
      }
    } else if (form::technology::GetMajor(tech) == form::technology::HDF5_MAJOR) {
#ifdef USE_HDF5_STORAGE
      // Add HDF5 implementation when available
      // return std::make_shared<HDF5_Read_ContainerImp>(name);
#endif // USE_HDF5_STORAGE
    }

    // Default fallback
    return std::make_shared<Storage_Read_Association>(name);
  }

  inline std::shared_ptr<IStorage_Write_Container> createWriteAssociation(int tech, std::string const& name)
  {
    if (form::technology::GetMajor(tech) == form::technology::ROOT_MAJOR) {
      if (form::technology::GetMinor(tech) == form::technology::ROOT_TTREE_MINOR) {
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TTree_Write_ContainerImp>(name);
#endif // USE_ROOT_STORAGE
      } else if (form::technology::GetMinor(tech) == form::technology::ROOT_RNTUPLE_MINOR) {
#ifdef USE_RNTUPLE_STORAGE
        //return std::make_shared<ROOT_RNTuple_Write_ContainerImp>(name);
#endif // USE_RNTUPLE_STORAGE
      }
    } else if (form::technology::GetMajor(tech) == form::technology::HDF5_MAJOR) {
#ifdef USE_HDF5_STORAGE
      // Add HDF5 implementation when available
      // return std::make_shared<HDF5_Write_ContainerImp>(name);
#endif // USE_HDF5_STORAGE
    }

    // Default fallback
    return std::make_shared<Storage_Write_Association>(name);
  }

  inline std::shared_ptr<IStorage_Read_Container> createReadContainer(int tech, std::string const& name)
  {
    // Use the helper functions from Technology namespace for consistency
    if (form::technology::GetMajor(tech) == form::technology::ROOT_MAJOR) {
      if (form::technology::GetMinor(tech) == form::technology::ROOT_TTREE_MINOR) {
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TBranch_Read_ContainerImp>(name);
#endif // USE_ROOT_STORAGE
      } else if (form::technology::GetMinor(tech) == form::technology::ROOT_RNTUPLE_MINOR) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<ROOT_RField_Read_ContainerImp>(name);
#endif // USE_RNTUPLE_STORAGE
      }
    } else if (form::technology::GetMajor(tech) == form::technology::HDF5_MAJOR) {
#ifdef USE_HDF5_STORAGE
      // Add HDF5 implementation when available
      // return std::make_shared<HDF5_Field_Read_ContainerImp>(name);
#endif // USE_HDF5_STORAGE
    }

    // Default fallback
    return std::make_shared<Storage_Read_Container>(name);
  }

  inline std::shared_ptr<IStorage_Write_Container> createWriteContainer(int tech, std::string const& name)
  {
    // Use the helper functions from Technology namespace for consistency
    if (form::technology::GetMajor(tech) == form::technology::ROOT_MAJOR) {
      if (form::technology::GetMinor(tech) == form::technology::ROOT_TTREE_MINOR) {
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TBranch_Write_ContainerImp>(name);
#endif // USE_ROOT_STORAGE
      } else if (form::technology::GetMinor(tech) == form::technology::ROOT_RNTUPLE_MINOR) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<ROOT_RField_Write_ContainerImp>(name);
#endif // USE_RNTUPLE_STORAGE
      }
    } else if (form::technology::GetMajor(tech) == form::technology::HDF5_MAJOR) {
#ifdef USE_HDF5_STORAGE
      // Add HDF5 implementation when available
      // return std::make_shared<HDF5_Field_Write_ContainerImp>(name);
#endif // USE_HDF5_STORAGE
    }

    // Default fallback
    return std::make_shared<Storage_Write_Container>(name);
  }

} // namespace form::detail::experimental
#endif // FORM_UTIL_FACTORIES_HPP
