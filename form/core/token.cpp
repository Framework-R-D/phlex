// Copyright (C) 2025 ...

#include "token.hpp"

#include <utility>

using namespace form::detail::experimental;

/// placement-only constructor; id is left unset
token::token(std::string file_name, std::string container_name, technology::id technology) :
  technology_(technology),
  file_name_(std::move(file_name)),
  container_name_(std::move(container_name)),
  id_(0),
  has_id_(false)
{
}

/// Fully-specified constructor; id is set
token::token(std::string file_name,
             std::string container_name,
             technology::id technology,
             std::uint64_t id) :
  technology_(technology),
  file_name_(std::move(file_name)),
  container_name_(std::move(container_name)),
  id_(id),
  has_id_(true)
{
}

/// Access file name
std::string const& token::file_name() const { return file_name_; }
/// Access container name
std::string const& token::container_name() const { return container_name_; }
/// Access technology type
form::technology::id token::technology() const { return technology_; }
/// Access identifier/entry number (0-based row)
std::uint64_t token::id() const { return id_; }
/// Whether an id has been set on this token
bool token::has_id() const { return has_id_; }
