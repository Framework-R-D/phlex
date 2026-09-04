// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_FACTORIES_HPP
#define FORM_STORAGE_FACTORIES_HPP

#include "core/technology.hpp"
#include "storage/istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  std::shared_ptr<i_storage_file> create_file(form::technology::id tech,
                                              std::string const& name,
                                              char mode);

  std::shared_ptr<i_storage_write_container> create_write_association(form::technology::id tech,
                                                                      std::string const& name);

  std::shared_ptr<i_storage_read_container> create_read_container(form::technology::id tech,
                                                                  std::string const& name);

  std::shared_ptr<i_storage_write_container> create_write_container(form::technology::id tech,
                                                                    std::string const& name);

} // namespace form::detail::experimental
#endif // FORM_STORAGE_FACTORIES_HPP
