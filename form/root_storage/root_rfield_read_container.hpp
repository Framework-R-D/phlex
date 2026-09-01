//A root_rfield_read_container is a storage_read_container that uses a shared RNTuple to read data products from disk.  A single storage_read_container encapsulates the location where a collection of data products of a single type is stored.

#ifndef FORM_ROOT_STORAGE_ROOT_RFIELD_READ_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_RFIELD_READ_CONTAINER_HPP

#include "storage/storage_read_container.hpp"

#include <memory>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;

namespace ROOT {
  class RNTupleReader;
  template <class FieldType>
  class RNTupleView;
  template <>
  class RNTupleView<void>;
}
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {
  class root_rfield_read_container_imp : public storage_read_container {
  public:
    root_rfield_read_container_imp(std::string const& name);
    ~root_rfield_read_container_imp()
      override; //Must not be defined in header because that requires definition of RNTupleReader, etc.

    //Rule of five
    root_rfield_read_container_imp(root_rfield_read_container_imp const& other) = delete;
    root_rfield_read_container_imp(root_rfield_read_container_imp&& other) = delete;
    root_rfield_read_container_imp& operator=(root_rfield_read_container_imp const& other) = delete;
    root_rfield_read_container_imp& operator=(root_rfield_read_container_imp&& other) = delete;

    void set_file(std::shared_ptr<i_storage_file> file) override;
    void prime(std::type_info const& type) override;
    bool read(int id, void const** data, std::type_info const& type) override;
    int entries() override;

  private:
    std::shared_ptr<TFile> tfile_;
    std::unique_ptr<ROOT::RNTupleReader> reader_;
    std::unique_ptr<ROOT::RNTupleView<void>> view_;

    void create_view(std::type_info const& type);
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RFIELD_READ_CONTAINER_HPP
