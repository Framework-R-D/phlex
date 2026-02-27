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
    form_reader_interface(config::output_item_config const& output_config,
                   config::tech_setting_config const& tech_config);
    ~form_reader_interface() = default;

    void read(std::string const& creator,
              std::string const& segment_id,
              product_with_name& product);

  private:
    std::unique_ptr<form::detail::experimental::IPersistenceReader> m_pers;
    std::map<std::string, form::experimental::config::PersistenceItem> m_product_to_config;
  };
}

#endif // FORM_FORM_FORM_READER_HPP
