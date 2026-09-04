// Copyright (C) 2025 ...

#include "form_reader.hpp"

#include <stdexcept>
#include <typeinfo>

namespace form::experimental {

  form_reader_interface::form_reader_interface(config::item_config const& config_item,
                                               config::tech_setting_config const& tech_config) :
    pers_reader_(nullptr)
  {
    for (auto const& item : config_item.get_items()) {
      product_to_config_.emplace(item.product_name,
                                 form::experimental::config::persistence_item(
                                   item.product_name, item.file_name, item.technology));
    }

    pers_reader_ = form::detail::experimental::create_persistence_reader();
    pers_reader_->configure(config_item);
    pers_reader_->configure_tech_settings(tech_config);
  }

  void form_reader_interface::read(std::string const& creator,
                                   std::string const& segment_id,
                                   product_with_name& product)
  {

    auto config_it = product_to_config_.find(product.label);
    if (config_it == product_to_config_.end()) {
      throw std::runtime_error("No configuration found for product: " + product.label);
    }

    pers_reader_->read(creator, product.label, segment_id, &product.data, *product.type);
  }

  void form_reader_interface::prime(std::string const& creator,
                                    std::string const& product_name,
                                    std::type_info const& type)
  {
    pers_reader_->prime(creator, product_name, type);
  }

  std::vector<std::string> form_reader_interface::indices(std::string const& creator,
                                                          std::string const& product_name)
  {
    return pers_reader_->list_indices(creator, product_name);
  }
}
