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

      ROOT::RNTupleWriter& get_writer();
      //get_model() also signals whether model_ has already been moved from.
      //If model_ has been moved from, the c++ standard guarantees it will contain nullptr.
      //This is important for this RNTuple backend to meet FORM's testing
      //requirement that commit() shall fail if fill() has not been called yet.
      std::unique_ptr<ROOT::RNTupleModel> const& get_model() const;
      RRawPtrWriteEntry& get_entry();

    private:
      std::shared_ptr<TFile> tfile_;

      //State shared by root_rfield_write_container_imps
      std::unique_ptr<ROOT::RNTupleWriter> writer_;
      std::unique_ptr<ROOT::RNTupleModel> model_;
      std::unique_ptr<RRawPtrWriteEntry> entry_;
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RNTUPLE_WRITE_CONTAINER_HPP
