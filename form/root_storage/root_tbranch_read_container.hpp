// Copyright (C) 2025 ...

#ifndef FORM_ROOT_STORAGE_ROOT_TBRANCH_READ_CONTAINER_HPP
#define FORM_ROOT_STORAGE_ROOT_TBRANCH_READ_CONTAINER_HPP

#include "storage/storage_read_container.hpp"

#include <memory>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
// Forward declarations of ROOT classes
class TFile;
class TTree;
class TBranch;
// NOLINTEND(readability-identifier-naming)

namespace form::detail::experimental {

  class root_tbranch_read_container_imp : public storage_read_container {
  public:
    explicit root_tbranch_read_container_imp(std::string const& name);
    ~root_tbranch_read_container_imp() override = default;

    void set_file(std::shared_ptr<i_storage_file> file) override;
    void prime(std::type_info const& type) override;

    bool read(int id, void const** data, std::type_info const& type) override;
    int entries() override;

  private:
    std::shared_ptr<TFile> tfile_;
    TTree* tree_{nullptr};
    TBranch* branch_{nullptr};
  };

} // namespace form::detail::experimental

#endif // FORM_ROOT_STORAGE_ROOT_TBRANCH_READ_CONTAINER_HPP
