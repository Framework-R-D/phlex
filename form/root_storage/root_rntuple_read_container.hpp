//A ROOT_RNTuple_ContainerImp is a Storage_Association (and therefore a Storage_Container) that coordinates the file accesses shared by several ROOT_RField_ContainerImps.  It only coordinates RNTuple-specific file-based resources and doesn't actually implement write() or read() for example.  This matches the early design of the TTree associative container.

#ifndef FORM_ROOT_STORAGE_ROOT_RNTUPLE_READ_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_RNTUPLE_READ_CONTAINER_HPP

#include "storage/storage_read_association.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  class ROOT_RNTuple_Read_ContainerImp : public Storage_Read_Association {
  public:
    ROOT_RNTuple_Read_ContainerImp(std::string const& name);
    ~ROOT_RNTuple_Read_ContainerImp() = default;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    bool read(int id, void const** data, std::type_info const& type) override;
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RNTUPLE_READ_CONTAINER_HPP
