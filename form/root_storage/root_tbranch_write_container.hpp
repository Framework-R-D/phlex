// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TBRANCH_WRITE_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TBRANCH_WRITE_CONTAINER_HPP

#include "storage/storage_associative_write_container.hpp"

#include <memory>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;
class TTree;
class TBranch;
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {

  class root_tbranch_write_container_imp : public storage_associative_write_container {
  public:
    explicit root_tbranch_write_container_imp(std::string const& name);
    ~root_tbranch_write_container_imp() override = default;

    void set_attribute(std::string const& key, std::string const& value) override;

    void set_file(std::shared_ptr<i_storage_file> file) override;
    void set_parent(std::shared_ptr<i_storage_write_container> parent) override;

    void setup_write(std::type_info const& type = typeid(void)) override;
    std::uint64_t fill(void const* data) override;
    void commit() override;

  private:
    std::shared_ptr<TFile> tfile_;
    TTree* tree_{nullptr};
    TBranch* branch_{nullptr};
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TBRANCH_WRITE_CONTAINER_HPP
