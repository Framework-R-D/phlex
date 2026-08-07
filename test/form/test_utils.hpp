//Utilities to make FORM unit tests easier to write and maintain

#ifndef TEST_FORM_TEST_UTILS_HPP
#define TEST_FORM_TEST_UTILS_HPP

#include "root_storage/demangle_name.hpp"
#include "storage/istorage.hpp"
#include "storage/storage_associative_write_container.hpp"
#include "storage/storage_read_container.hpp"
#include "util/factories.hpp"

#include <cstring>
#include <iostream>
#include <memory>

using namespace form::detail::experimental;

namespace form::test {

  inline constexpr char const* testTreeName = "FORMTestTree";
  inline constexpr char const* testFileName = "FORMTestFile.root";

  template <class PROD>
  inline std::string getTypeName()
  {
    return DemangleName(typeid(PROD));
  }

  template <class PROD>
  inline std::string makeTestBranchName()
  {
    auto branchName = std::string(testTreeName) + "/" + getTypeName<PROD>();
    for (size_t firstSpace = branchName.find_first_of(' '); firstSpace != std::string::npos;
         firstSpace = branchName.find_first_of(' ')) {
      branchName = branchName.erase(firstSpace, 1);
    }
    return branchName;
  }

  inline std::vector<std::shared_ptr<IStorage_Write_Container>> doWrite(
    std::shared_ptr<IStorage_File>& /*file*/,
    form::technology::Id const /*technology*/,
    std::shared_ptr<IStorage_Write_Container>& /*parent*/)
  {
    return {};
  }

  template <class PROD, class... PRODS>
  inline std::vector<std::shared_ptr<IStorage_Write_Container>> doWrite(
    std::shared_ptr<IStorage_File>& file,
    form::technology::Id const technology,
    std::shared_ptr<IStorage_Write_Container>& parent,
    PROD& prod,
    PRODS&... prods)
  {
    auto const branchName = makeTestBranchName<PROD>();
    auto container = createWriteContainer(technology, branchName);
    auto assoc = dynamic_pointer_cast<Storage_Associative_Write_Container>(container);
    if (assoc) {
      assoc->setParent(parent);
    }
    container->setFile(file);
    container->setupWrite(typeid(PROD));

    auto result = doWrite(file, technology, parent, prods...);
    container->fill(&prod); //This must happen after setupWrite()
    result.push_back(container);
    return result;
  }

  template <class... PRODS>
  inline void write(form::technology::Id const technology, PRODS&... prods)
  {
    auto file = createFile(technology, std::string(testFileName), 'o');
    auto parent = createWriteAssociation(technology, std::string(testTreeName));
    parent->setFile(file);
    parent->setupWrite();

    auto keepContainersAlive = doWrite(file, technology, parent, prods...);
    keepContainersAlive.back()
      ->commit(); //Elements are in reverse order of container construction, so this makes sure container owner calls commit()
  }

  template <class PROD>
  inline std::unique_ptr<PROD const> doRead(std::shared_ptr<IStorage_File>& file,
                                            form::technology::Id const technology)
  {
    auto container = createReadContainer(technology, makeTestBranchName<PROD>());
    container->setFile(file);
    void const* rawPtr = nullptr;

    if (!container->read(0, &rawPtr, typeid(PROD))) {
      throw std::runtime_error("Failed to read a " + getTypeName<PROD>());
    }

    return std::unique_ptr<PROD const>(static_cast<PROD const*>(rawPtr));
  }

  template <class... PRODS>
  inline std::tuple<std::unique_ptr<PRODS const>...> read(form::technology::Id const technology)
  {
    auto file = createFile(technology, std::string(testFileName), 'i');

    return std::make_tuple(doRead<PRODS>(file, technology)...);
  }

  inline form::technology::Id getTechnology(std::string const& tech_string)
  {
    return form::technology::from_string(tech_string);
  }

} // namespace form::test

#endif // TEST_FORM_TEST_UTILS_HPP
