//A ROOT_RField_Read_Container is a Storage_Read_Container that uses a shared RNTuple to read and write data products to and from disk.  A single Storage_Read_Container encapsulates the location where a collection of data products of a single type is stored.

#ifndef FORM_ROOT_STORAGE_ROOT_RFIELD_READ_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_RFIELD_READ_CONTAINER_HPP

#include "storage/storage_associative_read_container.hpp"

#include <memory>
#include <string>

class TFile;

namespace ROOT {
  class RNTupleReader;
  template <class FIELD_TYPE>
  class RNTupleView;
  class RNTupleView<void>;
}

namespace form::detail::experimental {
  class ROOT_RNTuple_Read_ContainerImp;

  class ROOT_RField_Read_ContainerImp : public Storage_Associative_Read_Container {
  public:
    ROOT_RField_Read_ContainerImp(std::string const& name);
    ~ROOT_RField_Read_ContainerImp();

    void setAttribute(std::string const& key, std::string const& value) override;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setParent(std::shared_ptr<IStorage_Read_Container> const parent) override;
    bool read(int id, void const** data, std::type_info const& type) override;

  private:
    std::shared_ptr<TFile> m_tfile;
    std::unique_ptr<ROOT::RNTupleReader> m_reader;
    std::unique_ptr<ROOT::RNTupleView<void>> m_view;

    bool m_force_streamer_field;
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RFIELD_READ_CONTAINER_HPP
