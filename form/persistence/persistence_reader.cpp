// Copyright (C) 2025 ...

#include "persistence_reader.hpp"
#include "persistence_utils.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

using namespace form::detail::experimental;

namespace form::detail::experimental {
  std::unique_ptr<i_persistence_reader> create_persistence_reader()
  {
    return std::make_unique<persistence_reader>();
  }
}

persistence_reader::persistence_reader() :
  store_reader_(create_storage_reader()),
  config_items_(),
  tech_settings_() // constructor takes form config
{
}

void persistence_reader::configure_tech_settings(
  form::experimental::config::tech_setting_config const& tech_config_settings)
{
  tech_settings_ = tech_config_settings;
}

void persistence_reader::configure(form::experimental::config::item_config const& config_items)
{
  config_items_ = config_items;
}

void persistence_reader::read(std::string const& creator,
                              std::string const& label,
                              std::string const& id,
                              void const** data,
                              std::type_info const& type)
{
  std::unique_ptr<token> token = get_token(creator, label, id);
  store_reader_->read_container(*token, data, type, tech_settings_);
}

void persistence_reader::prime(std::string const& creator,
                               std::string const& label,
                               std::type_info const& type)
{
  auto const config_item = find_config_item(config_items_, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = build_full_label(creator, label);
  store_reader_->prime(
    token{config_item->file_name, full_label, config_item->technology}, type, tech_settings_);
}

std::vector<std::string> persistence_reader::list_indices(std::string const& creator,
                                                          std::string const& label)
{
  auto const config_item = find_config_item(config_items_, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = build_full_label(creator, "index");
  return store_reader_->list_indices(
    token{config_item->file_name, full_label, config_item->technology}, tech_settings_);
}

std::unique_ptr<token> persistence_reader::get_token(std::string const& creator,
                                                     std::string const& label,
                                                     std::string const& id)
{
  auto const config_item = find_config_item(config_items_, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = build_full_label(creator, label);
  std::string const index_label = build_full_label(creator, "index");

  int const row_id = store_reader_->get_index(
    token{config_item->file_name, index_label, config_item->technology}, id, tech_settings_);
  return std::make_unique<token>(
    config_item->file_name, full_label, config_item->technology, row_id);
}
