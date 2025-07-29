// Copyright (C) 2025 ...

#include "storage_container.hpp"
#include "storage_file.hpp"

using namespace form::detail::experimental;

Storage_Container::Storage_Container(const std::string& name) : m_name(name), m_file(nullptr) {}

Storage_Container::~Storage_Container() {}

const std::string& Storage_Container::name() { return m_name; }

void Storage_Container::setFile(std::shared_ptr<IStorage_File> file) { m_file = file; }

void Storage_Container::setupWrite(const std::string& /* type*/) { return; }

void Storage_Container::fill(const void* /* data*/) { return; }

void Storage_Container::commit() { return; }

bool Storage_Container::read(int /* id*/, const void** /*data*/, std::string& /* type*/)
{
  return false;
}
