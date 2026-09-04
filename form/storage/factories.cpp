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

  using major = form::technology::major;

  std::shared_ptr<i_storage_file> create_file(form::technology::id tech,
                                              std::string const& name,
                                              char mode)
  {
    switch (tech.major) {
    case major::generic:
      // No technology specified: generic storage.
      return std::make_shared<storage_file>(name, mode);
    case major::root:
#ifdef USE_ROOT_STORAGE
      return std::make_shared<root_tfile_imp>(name, mode);
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

  std::shared_ptr<i_storage_write_container> create_write_association(form::technology::id tech,
                                                                      std::string const& name)
  {
    switch (tech.major) {
    case major::generic:
      // No technology specified: generic storage.
      return std::make_shared<storage_write_association>(name);
    case major::root:
#ifdef USE_ROOT_STORAGE
      if (tech == form::technology::root_ttree) {
        return std::make_shared<root_ttree_write_container_imp>(name);
      } else if (tech == form::technology::root_rntuple) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<root_rntuple_write_container_imp>(name);
#else
        throw std::runtime_error("FORM: ROOT RNTUPLE support is not compiled into this build");
#endif
      }
      // Recognized ROOT major, but an unsupported subtype/minor.
      throw std::runtime_error("FORM: requested ROOT write-association backend is not available");
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

  std::shared_ptr<i_storage_read_container> create_read_container(form::technology::id tech,
                                                                  std::string const& name)
  {
    switch (tech.major) {
    case major::generic:
      // No technology specified: generic storage.
      return std::make_shared<storage_read_container>(name);
    case major::root:
#ifdef USE_ROOT_STORAGE
      if (tech == form::technology::root_ttree) {
        return std::make_shared<root_tbranch_read_container_imp>(name);
      } else if (tech == form::technology::root_rntuple) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<root_rfield_read_container_imp>(name);
#else
        throw std::runtime_error("FORM: ROOT RNTUPLE support is not compiled into this build");
#endif
      }
      // Recognized ROOT major, but an unsupported subtype/minor.
      throw std::runtime_error("FORM: requested ROOT read-container backend is not available");
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

  std::shared_ptr<i_storage_write_container> create_write_container(form::technology::id tech,
                                                                    std::string const& name)
  {
    switch (tech.major) {
    case major::generic:
      // No technology specified: generic storage.
      return std::make_shared<storage_write_container>(name);
    case major::root:
#ifdef USE_ROOT_STORAGE
      if (tech == form::technology::root_ttree) {
        return std::make_shared<root_tbranch_write_container_imp>(name);
      } else if (tech == form::technology::root_rntuple) {
#ifdef USE_RNTUPLE_STORAGE
        return std::make_shared<root_rfield_write_container_imp>(name);
#else
        throw std::runtime_error("FORM: ROOT RNTUPLE support is not compiled into this build");
#endif
      }
      // Recognized ROOT major, but an unsupported subtype/minor.
      throw std::runtime_error("FORM: requested ROOT write-container backend is not available");
#else
      throw std::runtime_error("FORM: ROOT support is not compiled into this build");
#endif
    case major::hdf5:
      throw std::runtime_error("FORM: HDF5 storage is recognized but not yet implemented");
    }
    throw std::runtime_error("FORM: unsupported storage technology requested");
  }

} // namespace form::detail::experimental
