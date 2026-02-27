// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_STORAGE_HPP
#define FORM_ROOT_STORAGE_ROOT_STORAGE_HPP

#include "storage/storage.hpp"

namespace form::detail::experimental {

  class ROOT_StorageImp : public Storage {
  public:
    ROOT_StorageImp() = default;
    ~ROOT_StorageImp() = default;
  };

} //namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_STORAGE_HPP
