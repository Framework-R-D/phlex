//A root_rntuple_write_container_imp is a storage_write_association (and therefore a storage_container) that coordinates the file accesses shared by several root_rfield_write_container_imps.  It only coordinates RNTuple-specific file-based resources and doesn't actually implement write() or read() for example.  This matches the early design of the TTree associative container.

#ifndef FORM_ROOT_STORAGE_ROOT_RNTUPLE_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_RNTUPLE_WRITE_CONTAINER_HPP

#include "storage/storage_write_association.hpp"

#include "RVersion.h"

#include <memory>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;

namespace ROOT {
  class RNTupleWriter;
  class RNTupleModel;

#if ROOT_VERSION_CODE >= ROOT_VERSION(6, 40, 0)
  namespace Detail {
    class RRawPtrWriteEntry;
  }
#else
  namespace Experimental {
    namespace Detail {
      class RRawPtrWriteEntry;
    }
  }
#endif
}
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {

  //ROOT 6.40 moved RRawPtrWriteEntry from ROOT::Experimental::Detail to ROOT::Detail.
#if ROOT_VERSION_CODE >= ROOT_VERSION(6, 40, 0)
  // NOLINTNEXTLINE(readability-identifier-naming)
  using RRawPtrWriteEntry = ROOT::Detail::RRawPtrWriteEntry;
#else
  // NOLINTNEXTLINE(readability-identifier-naming)
  using RRawPtrWriteEntry = ROOT::Experimental::Detail::RRawPtrWriteEntry;
#endif

  class root_rntuple_write_container_imp : public storage_write_association {
  public:
    root_rntuple_write_container_imp(std::string const& name);
    ~root_rntuple_write_container_imp() override;
    //Rule of five
    root_rntuple_write_container_imp(root_rntuple_write_container_imp const& other) = delete;
    root_rntuple_write_container_imp(root_rntuple_write_container_imp&& other) = delete;
    root_rntuple_write_container_imp& operator=(root_rntuple_write_container_imp const& other) =
      delete;
    root_rntuple_write_container_imp& operator=(root_rntuple_write_container_imp&& other) = delete;

    void set_file(std::shared_ptr<i_storage_file> file) override;
    void setup_write(std::type_info const& type) override;
    std::uint64_t fill(void const* data) override;
    void commit() override;

    //State shared by root_rfield_write_container_imps
    std::unique_ptr<ROOT::RNTupleWriter> writer;
    std::unique_ptr<ROOT::RNTupleModel> model;
    std::unique_ptr<RRawPtrWriteEntry> entry;
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RNTUPLE_WRITE_CONTAINER_HPP
