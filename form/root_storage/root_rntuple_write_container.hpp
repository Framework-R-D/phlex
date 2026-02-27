//A ROOT_RNTuple_ContainerImp is a Storage_Write_Association (and therefore a Storage_Container) that coordinates the file accesses shared by several ROOT_RField_ContainerImps.  It only coordinates RNTuple-specific file-based resources and doesn't actually implement write() or read() for example.  This matches the early design of the TTree associative container.

#ifndef FORM_ROOT_STORAGE_ROOT_RNTUPLE_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_RNTUPLE_WRITE_CONTAINER_HPP

#include "storage/storage_write_association.hpp"

#include <memory>
#include <string>

class TFile;

namespace ROOT {
  class RNTupleWriter;
  class RNTupleModel;

  namespace Experimental {
    namespace Detail {
      class RRawPtrWriteEntry;
    }
  }
}

namespace form::detail::experimental {

  class ROOT_RNTuple_Write_ContainerImp : public Storage_Write_Association {
  public:
    ROOT_RNTuple_Write_ContainerImp(std::string const& name);
    ~ROOT_RNTuple_Write_ContainerImp();

    void setFile(std::shared_ptr<IStorage_File> file) override;
    void setupWrite(std::type_info const& type) override;
    void fill(void const* data) override;
    void commit() override;

    //State shared by ROOT_RField_ContainerImps
    std::unique_ptr<ROOT::RNTupleWriter> m_writer;
    std::unique_ptr<ROOT::RNTupleModel> m_model;
    std::unique_ptr<ROOT::Experimental::Detail::RRawPtrWriteEntry> m_entry;
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RNTUPLE_WRITE_CONTAINER_HPP
