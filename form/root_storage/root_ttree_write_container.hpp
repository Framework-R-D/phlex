// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TTREE_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TTREE_WRITE_CONTAINER_HPP

#include "storage/storage_write_association.hpp"

#include <memory>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;
class TTree;
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {

  class root_ttree_write_container_imp : public storage_write_association {
  public:
    explicit root_ttree_write_container_imp(std::string const& name);
    ~root_ttree_write_container_imp() override = default;

    root_ttree_write_container_imp(root_ttree_write_container_imp const&) = delete;
    root_ttree_write_container_imp& operator=(root_ttree_write_container_imp const&) = delete;
    root_ttree_write_container_imp(root_ttree_write_container_imp&&) = delete;
    root_ttree_write_container_imp& operator=(root_ttree_write_container_imp&&) = delete;

    void set_file(std::shared_ptr<i_storage_file> file) override;
    void setup_write(std::type_info const& type) override;
    std::uint64_t fill(void const* data) override;
    void commit() override;

    TTree* get_ttree();

  private:
    // Be absolutely explicit about the ownership semantics of TTree*,
    // since ROOT nominally manages the memory of TTree objects, but in
    // practice we need to ensure objects are written to the TTree—and
    // the TTree itself is deleted—at the right time i.e. before its
    //  containing TFile is deleted.
    struct ttree_deleter {
      void operator()(TTree* t) const;
    };

    // Ordering of members is critical here:
    // the TTree must be deleted before the TFile.
    std::shared_ptr<TFile> tfile_;
    std::unique_ptr<TTree, ttree_deleter> tree_;
  };

} //namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TTREE_WRITE_CONTAINER_HPP
