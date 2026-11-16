// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_DESCRIPTOR_HPP
#define FORM_STORAGE_DESCRIPTOR_HPP

#include "core/technology.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* @file descriptor.hpp
 * @brief The FORM layout descriptor: its schema and storage abstraction.
 *
 * The descriptor is a file's bootstrap record: it identifies the layout used to interpret the rest
 * of the file. Each storage backend derives from `descriptor` to store the record in its own native
 * form, while the schema, unique-key rule and keyed (rather than ordered) read contract are defined
 * here, ensuring consistent behavior across backends.
 */
namespace form::detail::experimental {

  inline constexpr std::string_view form_block_prefix = "form_";

  /// Backend-native object name reserved for the FORM layout descriptor.
  inline constexpr std::string_view descriptor_object_name = "form_descriptor";

  /// On-disk layout version, independent of the Phlex release version.
  inline constexpr std::string_view form_layout_version = "0.1";

  /// Keys written into the descriptor.
  inline constexpr std::string_view descriptor_layout_version_key = "form_layout_version";
  inline constexpr std::string_view descriptor_technologies_key = "technologies";

  inline constexpr char descriptor_technology_separator = ',';

  /// A descriptor's payload: key/value entries whose keys are unique.
  using descriptor_entries = std::vector<std::pair<std::string, std::string>>;

  /// Throws if the file declares an unsupported layout version.
  void check_layout_version(std::string_view version, std::string const& file_name);

  std::vector<form::technology::id> parse_technologies(std::string_view text,
                                                       std::string const& file_name);

  std::optional<std::string> descriptor_entry(descriptor_entries const& entries,
                                              std::string_view key);

  /// A file's layout descriptor, independent of how a backend stores it.
  class descriptor {
  public:
    explicit descriptor(std::string file_name);
    virtual ~descriptor() = default;

    descriptor(descriptor const&) = delete;
    descriptor& operator=(descriptor const&) = delete;
    descriptor(descriptor&&) = delete;
    descriptor& operator=(descriptor&&) = delete;

    /// Stores the entries, rejecting duplicate keys consistently across all backends.
    void write(descriptor_entries const& entries);

    /// Returns std::nullopt if the file has no descriptor. Entry order is not guaranteed; callers
    /// must look up entries by key rather than rely on their position.
    virtual std::optional<descriptor_entries> read() = 0;

    std::string const& file_name() const;

  protected:
    /// Stores entries whose keys write() has already found to be unique.
    virtual void store(descriptor_entries const& entries) = 0;

  private:
    std::string file_name_;
  };

  /// Keeps the descriptor in memory, for the generic backend that writes no file.
  class memory_descriptor : public descriptor {
  public:
    using descriptor::descriptor;

    std::optional<descriptor_entries> read() override;

  protected:
    void store(descriptor_entries const& entries) override;

  private:
    std::optional<descriptor_entries> entries_;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_DESCRIPTOR_HPP
