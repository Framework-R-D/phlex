// Copyright (C) 2025 .....

#ifndef __FACTORIES_H__
#define __FACTORIES_H__

#include "storage/istorage.hpp"

#include "storage/storage_association.hpp"
#include "storage/storage_container.hpp"
#include "storage/storage_file.hpp"

#include "root_storage/root_tbranch_container.hpp"
#include "root_storage/root_tfile.hpp"
#include "root_storage/root_ttree_container.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {
  std::shared_ptr<IStorage_File> createFile(int tech, const std::string& name, char mode)
  {
    if (int(tech / 256) == 1) { //ROOT major technology
#ifdef USE_ROOT_STORAGE
      return std::make_shared<ROOT_TFileImp>(name, mode);
#endif
    }
    return std::make_shared<Storage_File>(name, mode);
  }

  std::shared_ptr<IStorage_Container> createAssociation(int tech, const std::string& name)
  {
    if (int(tech / 256) == 1) {   //ROOT major technology
      if (int(tech % 256) == 1) { //ROOT TTree minor technology
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TTree_ContainerImp>(name);
#endif
      }
    }
    return std::make_shared<Storage_Association>(name);
  }

  std::shared_ptr<IStorage_Container> createContainer(int tech, const std::string& name)
  {
    if (int(tech / 256) == 1) {   //ROOT major technology
      if (int(tech % 256) == 1) { //ROOT TTree minor technology
#ifdef USE_ROOT_STORAGE
        return std::make_shared<ROOT_TBranch_ContainerImp>(name);
#endif
      }
    }
    return std::make_shared<Storage_Container>(name);
  }
} // namespace form::detail::experimental

#endif
