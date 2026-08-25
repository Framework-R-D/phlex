// Copyright (C) 2025 ...

#include "form_writer.hpp"

#include <iostream>
#include <stdexcept>
#include <typeinfo>

namespace form::experimental {

  form_writer_interface::form_writer_interface(config::item_config const& config_item,
                                               config::tech_setting_config const& tech_config) :
    pers_writer_(nullptr)
  {
    for (auto const& item : config_item.get_items()) {
      product_to_config_.emplace(item.product_name,
                                 form::experimental::config::persistence_item(
                                   item.product_name, item.file_name, item.technology));
    }

    pers_writer_ = form::detail::experimental::create_persistence_writer();
    pers_writer_->configure(config_item);
    pers_writer_->configure_tech_settings(tech_config);
  }

  void form_writer_interface::write(std::string const& creator,
                                    std::string const& segment_id,
                                    product_with_name const& product)
  {

    auto config_it = product_to_config_.find(product.label);
    if (config_it == product_to_config_.end()) {
      std::cerr << "No configuration found for product: " << product.label << '\n';
      return;
    }

    std::map<std::string, std::type_info const*> products = {{product.label, product.type}};
    pers_writer_->create_containers(creator, products);

    pers_writer_->register_write(creator, product.label, product.data, *product.type);

    pers_writer_->commit_output(creator, segment_id);
  }

  void form_writer_interface::write(std::string const& creator,
                                    std::string const& segment_id,
                                    std::vector<product_with_name> const& products)
  {

    if (products.empty()) {
      return;
    }

    auto config_it = product_to_config_.find(products[0].label);
    if (config_it == product_to_config_.end()) {
      std::cerr << "No configuration found for product: " << products[0].label << '\n';
      return;
    }

    // FIXME: Really only needed on first call
    std::map<std::string, std::type_info const*> product_types;
    for (auto const& pb : products) {
      product_types.insert(std::make_pair(pb.label, pb.type));
    }

    pers_writer_->create_containers(creator, product_types);

    for (auto const& pb : products) {
      // FIXME: We could consider checking id to be identical for all product bases here
      pers_writer_->register_write(creator, pb.label, pb.data, *pb.type);
    }

    pers_writer_->commit_output(creator, segment_id);
  }

}
