// Copyright (C) 2025 ...

#ifndef __ROOT_STORAGE_H__
#define __ROOT_STORAGE_H__

#include "storage/storage.hpp"

namespace form::detail::experimental {

  class ROOT_StorageImp : public Storage {
  public:
    ROOT_StorageImp() = default;
    ~ROOT_StorageImp() = default;
  };

} //namespace form::detail::experimental

#endif
