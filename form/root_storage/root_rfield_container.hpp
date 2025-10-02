//A ROOT_RField_Container is a Storage_Container that uses a shared RNTuple to read and write data products to and from disk.  A single Storage_Container encapsulates the location where a collection of data products of a single type is stored.

#ifndef __ROOT_RFIELD_CONTAINER_H__
#define __ROOT_RFIELD_CONTAINER_H__

#include "storage/storage_associative_container.hpp"

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
  class ROOT_RNTuple_ContainerImp;

  class ROOT_RField_ContainerImp : public Storage_Associative_Container {
  public:
    ROOT_RField_ContainerImp(std::string const& name);
    ~ROOT_RField_ContainerImp();

    void setAttribute(std::string const& key, std::string const& value) override;

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setupWrite(std::string const& type) override;
    void setParent(std::shared_ptr<IStorage_Container> const parent) override;
    void fill(void const* data) override;
    void commit() override;
    bool read(int id, void const** data, std::string& type) override;

  private:
    std::shared_ptr<TFile> m_tfile;
    std::unique_ptr<ROOT::RNTupleReader> m_reader;
    std::unique_ptr<ROOT::RNTupleView<void>> m_view;

    std::shared_ptr<ROOT_RNTuple_ContainerImp> m_rntuple_parent;

    bool m_force_streamer_field;
  };
}

#endif
