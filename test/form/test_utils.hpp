//Utilities to make FORM unit tests easier to write and maintain

#ifndef TEST_FORM_TEST_UTILS_HPP
#define TEST_FORM_TEST_UTILS_HPP

#include "root_storage/demangle_name.hpp"
#include "storage/factories.hpp"
#include "storage/istorage.hpp"
#include "storage/storage_associative_write_container.hpp"
#include "storage/storage_read_container.hpp"

#include <cstring>
#include <iostream>
#include <memory>

using namespace form::detail::experimental;

namespace form::test {

  inline constexpr char const* test_tree_name = "FORMTestTree";
  inline constexpr char const* test_file_name = "FORMTestFile.root";

  template <class Prod>
  inline std::string get_type_name()
  {
    return demangle_name(typeid(Prod));
  }

  template <class Prod>
  inline std::string make_test_branch_name()
  {
    auto branch_name = std::string(test_tree_name) + "/" + get_type_name<Prod>();
    for (size_t first_space = branch_name.find_first_of(' '); first_space != std::string::npos;
         first_space = branch_name.find_first_of(' ')) {
      branch_name = branch_name.erase(first_space, 1);
    }
    return branch_name;
  }

  inline std::vector<std::shared_ptr<i_storage_write_container>> do_write(
    std::shared_ptr<i_storage_file>& /*file*/,
    form::technology::id const /*technology*/,
    std::shared_ptr<i_storage_write_container>& /*parent*/)
  {
    return {};
  }

  template <class Prod, class... Prods>
  inline std::vector<std::shared_ptr<i_storage_write_container>> do_write(
    std::shared_ptr<i_storage_file>& file,
    form::technology::id const technology,
    std::shared_ptr<i_storage_write_container>& parent,
    Prod& prod,
    Prods&... prods)
  {
    auto const branch_name = make_test_branch_name<Prod>();
    auto container = create_write_container(technology, branch_name);
    auto assoc = dynamic_pointer_cast<storage_associative_write_container>(container);
    if (assoc) {
      assoc->set_parent(parent);
    }
    container->set_file(file);
    container->setup_write(typeid(Prod));

    auto result = do_write(file, technology, parent, prods...);
    container->fill(&prod); //This must happen after setup_write()
    result.push_back(container);
    return result;
  }

  template <class... Prods>
  inline void write(form::technology::id const technology, Prods&... prods)
  {
    auto file = create_file(technology, std::string(test_file_name), 'o');
    auto parent = create_write_association(technology, std::string(test_tree_name));
    parent->set_file(file);
    parent->setup_write();

    auto keep_containers_alive = do_write(file, technology, parent, prods...);
    keep_containers_alive.back()
      ->commit(); //Elements are in reverse order of container construction, so this makes sure container owner calls commit()
  }

  template <class Prod>
  inline std::unique_ptr<Prod const> do_read(std::shared_ptr<i_storage_file>& file,
                                             form::technology::id const technology)
  {
    auto container = create_read_container(technology, make_test_branch_name<Prod>());
    container->set_file(file);
    void const* raw_ptr = nullptr;

    if (!container->read(0, &raw_ptr, typeid(Prod))) {
      throw std::runtime_error("Failed to read a " + get_type_name<Prod>());
    }

    return std::unique_ptr<Prod const>(static_cast<Prod const*>(raw_ptr));
  }

  template <class... Prods>
  inline std::tuple<std::unique_ptr<Prods const>...> read(form::technology::id const technology)
  {
    auto file = create_file(technology, std::string(test_file_name), 'i');

    return std::make_tuple(do_read<Prods>(file, technology)...);
  }

  inline form::technology::id get_technology(std::string const& tech_string)
  {
    return form::technology::from_string(tech_string);
  }

} // namespace form::test

#endif // TEST_FORM_TEST_UTILS_HPP
