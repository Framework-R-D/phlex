// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_FACTORIES_HPP
#define FORM_STORAGE_FACTORIES_HPP

#include "core/technology.hpp"
#include "storage/istorage.hpp"

#include <memory>
#include <string>

namespace form::detail::experimental {

  std::shared_ptr<IStorage_File> createFile(form::technology::Id tech,
                                            std::string const& name,
                                            char mode);

  std::shared_ptr<IStorage_Write_Container> createWriteAssociation(form::technology::Id tech,
                                                                   std::string const& name);

  std::shared_ptr<IStorage_Read_Container> createReadContainer(form::technology::Id tech,
                                                               std::string const& name);

  std::shared_ptr<IStorage_Write_Container> createWriteContainer(form::technology::Id tech,
                                                                 std::string const& name);

} // namespace form::detail::experimental
#endif // FORM_STORAGE_FACTORIES_HPP
