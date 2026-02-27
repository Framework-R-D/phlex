//A ROOT_RField_Write_Container is a Storage_Write_Container that uses a shared RNTuple to write data products to disk.  A single Storage_Write_Container encapsulates the location where a collection of data products of a single type is stored.

#ifndef FORM_ROOT_STORAGE_ROOT_RFIELD_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_RFIELD_WRITE_CONTAINER_HPP

#include "storage/storage_associative_write_container.hpp"

#include <memory>
#include <string>

class TFile;

namespace form::detail::experimental {
  class ROOT_RNTuple_Write_ContainerImp;

  class ROOT_RField_Write_ContainerImp : public Storage_Associative_Write_Container {
  public:
    ROOT_RField_Write_ContainerImp(std::string const& name);
    ~ROOT_RField_Write_ContainerImp();

    void setAttribute(std::string const& key, std::string const& value) override;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setupWrite(std::type_info const& type) override;
    void setParent(std::shared_ptr<IStorage_Write_Container> const parent) override;
    void fill(void const* data) override;
    void commit() override;

  private:
    std::shared_ptr<TFile> m_tfile;
    std::shared_ptr<ROOT_RNTuple_Write_ContainerImp> m_rntuple_parent;

    bool m_force_streamer_field;
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RFIELD_WRITE_CONTAINER_HPP
