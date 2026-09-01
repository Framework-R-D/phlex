//A root_rfield_write_container is a storage_write_container that uses a shared RNTuple to write data products to disk.  A single storage_write_container encapsulates the location where a collection of data products of a single type is stored.

#ifndef FORM_ROOT_STORAGE_ROOT_RFIELD_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_RFIELD_WRITE_CONTAINER_HPP

#include "storage/storage_associative_write_container.hpp"

#include <memory>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {
  struct root_rntuple_write_container_imp;

  class root_rfield_write_container_imp : public storage_associative_write_container {
  public:
    root_rfield_write_container_imp(std::string const& name);
    ~root_rfield_write_container_imp() override = default;

    void set_attribute(std::string const& key, std::string const& value) override;

    void set_file(std::shared_ptr<i_storage_file> file) override;
    void setup_write(std::type_info const& type) override;
    void set_parent(std::shared_ptr<i_storage_write_container> const parent) override;
    std::uint64_t fill(void const* data) override;
    void commit() override;

  private:
    std::shared_ptr<TFile> tfile_;
    std::shared_ptr<root_rntuple_write_container_imp> rntuple_parent_;

    bool force_streamer_field_ = false;
  };
}

#endif // FORM_ROOT_STORAGE_ROOT_RFIELD_WRITE_CONTAINER_HPP
