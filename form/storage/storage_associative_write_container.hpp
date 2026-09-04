// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_ASSOCIATIVE_WRITE_CONTAINER_HPP
#define FORM_STORAGE_STORAGE_ASSOCIATIVE_WRITE_CONTAINER_HPP

#include "storage_write_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class storage_associative_write_container : public storage_write_container {
  public:
    explicit storage_associative_write_container(std::string const& name);
    ~storage_associative_write_container() override;

    std::string const& top_name();
    std::string const& col_name();

    virtual void set_parent(std::shared_ptr<i_storage_write_container> parent);

  private:
    std::string t_name_;
    std::string c_name_;

    std::shared_ptr<i_storage_write_container> parent_;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_ASSOCIATIVE_WRITE_CONTAINER_HPP
