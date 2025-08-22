// Copyright (C) 2025 .....
#ifndef __FACTORIES_HPP__
#define __FACTORIES_HPP__

#include "form/technology.hpp"
#include "storage/istorage.hpp"
#include "storage/storage_association.hpp"
#include "storage/storage_container.hpp"
#include "storage/storage_file.hpp"

#ifdef USE_ROOT_STORAGE
#include "root_storage/root_tbranch_container.hpp"
#include "root_storage/root_tfile.hpp"
#include "root_storage/root_ttree_container.hpp"
#endif

#include <memory>
#include <string>

namespace form::detail::experimental {

  std::shared_ptr<IStorage_File> createFile(int tech, const std::string& name, char mode)
  {
    if (form::Technology::GetMajor(tech) == form::Technology::ROOT_MAJOR) {
#ifdef USE_ROOT_STORAGE
      return std::make_shared<ROOT_TFileImp>(name, mode);
#endif
    }
    return std::make_shared<Storage_File>(name, mode);
  }

  std::shared_ptr<IStorage_Container> createAssociation(int tech, const std::string& name)
  {
    if (form::Technology::GetMajor(tech) == form::Technology::ROOT_MAJOR) {
      if (form::Technology::GetMinor(tech) == form::Technology::ROOT_TTREE_MINOR) {
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TTree_ContainerImp>(name);
#endif
      }
    }
    return std::make_shared<Storage_Association>(name);
  }

  std::shared_ptr<IStorage_Container> createContainer(int tech, const std::string& name)
  {
    if (form::Technology::GetMajor(tech) == form::Technology::ROOT_MAJOR) {
      if (form::Technology::GetMinor(tech) == form::Technology::ROOT_TTREE_MINOR) {
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TBranch_ContainerImp>(name);
#endif
      }
    }
    return std::make_shared<Storage_Container>(name);
  }

} // namespace form::detail::experimental
#endif
