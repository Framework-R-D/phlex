// Copyright (C) 2025 ...

#ifndef FORM_FORM_FORM_READER_HPP
#define FORM_FORM_FORM_READER_HPP

#include "form/config.hpp"
#include "form/product_with_name.hpp"
#include "persistence/ipersistence_reader.hpp"

#include <map>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

namespace form::experimental {

  class form_reader_interface {
  public:
    form_reader_interface(config::item_config const& config_item,
                          config::tech_setting_config const& tech_config);
    ~form_reader_interface() = default;

    void read(std::string const& creator,
              std::string const& segment_id,
              product_with_name& product);

    void prime(std::string const& creator,
               std::string const& product_name,
               std::type_info const& type);

    std::vector<std::string> indices(std::string const& creator, std::string const& product_name);

  private:
    std::unique_ptr<form::detail::experimental::i_persistence_reader> pers_reader_;
    std::map<std::string, form::experimental::config::persistence_item> product_to_config_;
  };
}

#endif // FORM_FORM_FORM_READER_HPP
