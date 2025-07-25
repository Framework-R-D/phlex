// Copyright (C) 2025 .....

#ifndef __FACTORIES_H__
#define __FACTORIES_H__

#include "storage/istorage.hpp"

#include "storage/storage_container.hpp"
#include "storage/storage_file.hpp"

#include "root_storage/root_tfile.hpp"
#include "root_storage/root_ttree_container.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {
  std::shared_ptr<IStorage_File> createFile(int tech, const std::string& name, char mode)
  {
    if (int(tech / 256) == 1) { //ROOT major technology
#ifdef USE_ROOT_STORAGE
      return std::shared_ptr<IStorage_File>(new ROOT_TFileImp(name, mode));
#endif
    }
    return std::shared_ptr<IStorage_File>(new Storage_File(name, mode));
  }
  
  std::shared_ptr<IStorage_Container> createContainer(int tech, const std::string& name)
  {
    if (int(tech / 256) == 1) {   //ROOT major technology
      if (int(tech % 256) == 1) { //ROOT TTree minor technology
#ifdef USE_ROOT_STORAGE
	return std::shared_ptr<IStorage_Container>(new ROOT_TTree_ContainerImp(name));
#endif
      }
    }
    return std::shared_ptr<IStorage_Container>(new Storage_Container(name));
  }
}

#endif
