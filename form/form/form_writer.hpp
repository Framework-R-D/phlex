// Copyright (C) 2025 ...

#ifndef FORM_FORM_FORM_WRITER_HPP
#define FORM_FORM_FORM_WRITER_HPP

#include "form/config.hpp"
#include "form/product_with_name.hpp"
#include "persistence/ipersistence_writer.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace form::experimental {

  class form_writer_interface {
  public:
    form_writer_interface(config::ItemConfig const& config_item,
                          config::tech_setting_config const& tech_config);
    ~form_writer_interface() = default;

    void write(std::string const& creator,
               std::string const& segment_id,
               product_with_name const& product);

    void write(std::string const& creator,
               std::string const& segment_id,
               std::vector<product_with_name> const& products);

    void finalize();

    // Explicitly declare the logical ProductName for a given routing label.
    // If not called, ProductName defaults to the creator name at write time.
    void declare_product_name(std::string const& routing_label, std::string const& product_name);

  private:
    std::unique_ptr<form::detail::experimental::IPersistenceWriter> m_pers_writer;
    std::map<std::string, form::experimental::config::PersistenceItem> m_product_to_config;
    std::map<std::string, std::string> m_label_to_product_name;
    bool m_finalized = false;
  };
}

#endif // FORM_FORM_FORM_WRITER_HPP
