// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_ISTORAGE_HPP
#define FORM_STORAGE_ISTORAGE_HPP

#include "core/placement.hpp"
#include "core/token.hpp"
#include "form/config.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace form::detail::experimental {

  // Sentinel returned by the write chain when no addressable row was written
  // (e.g. the generic no-op container). A real row is always < this value.
  inline constexpr std::uint64_t invalid_row_id = std::numeric_limits<std::uint64_t>::max();

  class i_storage_reader {
  public:
    i_storage_reader() = default;
    virtual ~i_storage_reader() = default;

    virtual int get_index(token const& token,
                          std::string const& id,
                          form::experimental::config::tech_setting_config const& settings) = 0;
    virtual void prime(token const& token,
                       std::type_info const& type,
                       form::experimental::config::tech_setting_config const& settings) = 0;
    virtual std::vector<std::string> list_indices(
      token const& token, form::experimental::config::tech_setting_config const& settings) = 0;
    virtual void read_container(
      token const& token,
      void const** data,
      std::type_info const& type,
      form::experimental::config::tech_setting_config const& settings) = 0;
  };

  class i_storage_writer {
  public:
    i_storage_writer() = default;
    virtual ~i_storage_writer() = default;

    virtual void create_containers(
      std::map<std::unique_ptr<placement>, std::type_info const*> const& containers,
      form::experimental::config::tech_setting_config const& settings) = 0;
    // Returns the 0-based row (entry) number written, or invalid_row_id if no rows
    virtual std::uint64_t fill_container(placement const& plcmnt,
                                         void const* data,
                                         std::type_info const& type) = 0;
    virtual void commit_containers(placement const& plcmnt) = 0;
  };

  class i_storage_file {
  public:
    i_storage_file() = default;
    virtual ~i_storage_file() = default;

    virtual std::string const& name() = 0;
    virtual char mode() = 0;

    virtual void set_attribute(std::string const& name, std::string const& value) = 0;
  };

  class i_storage_write_container {
  public:
    i_storage_write_container() = default;
    virtual ~i_storage_write_container() = default;

    virtual std::string const& name() = 0;

    virtual void set_file(std::shared_ptr<i_storage_file> file) = 0;
    virtual void setup_write(std::type_info const& type = typeid(void)) = 0;
    // Returns the 0-based row (entry) number written, or invalid_row_id if no rows
    virtual std::uint64_t fill(void const* data) = 0;
    virtual void commit() = 0;

    virtual void set_attribute(std::string const& name, std::string const& value) = 0;
  };

  class i_storage_read_container {
  public:
    i_storage_read_container() = default;
    virtual ~i_storage_read_container() = default;

    virtual std::string const& name() = 0;

    virtual void set_file(std::shared_ptr<i_storage_file> file) = 0;
    virtual void prime(std::type_info const& type) = 0;
    virtual bool read(int id, void const** data, std::type_info const& type) = 0;
    virtual int entries() = 0;

    virtual void set_attribute(std::string const& name, std::string const& value) = 0;
  };

  std::unique_ptr<i_storage_reader> create_storage_reader();
  std::unique_ptr<i_storage_writer> create_storage_writer();

} // namespace form::detail::experimental

#endif // FORM_STORAGE_ISTORAGE_HPP
