//Utilities to make FORM unit tests easier to write and maintain

#ifndef TEST_FORM_TEST_UTILS_HPP
#define TEST_FORM_TEST_UTILS_HPP

#include "storage/istorage.hpp"
#include "storage/storage_read_container.hpp"
#include "storage/storage_associative_write_container.hpp"
#include "util/factories.hpp"

#include "TClass.h"

#include <iostream>
#include <memory>

using namespace form::detail::experimental;

namespace form::test {

  inline std::string const testTreeName = "FORMTestTree";
  inline std::string const testFileName = "FORMTestFile.root";

  template <class PROD>
  inline std::string getTypeName()
  {
    return TClass::GetClass<PROD>()->GetName();
  }

  template <class PROD>
  inline std::string makeTestBranchName()
  {
    return testTreeName + "/" + getTypeName<PROD>();
  }

  inline std::vector<std::shared_ptr<IStorage_Write_Container>> doWrite(
    std::shared_ptr<IStorage_File>& /*file*/,
    int const /*technology*/,
    std::shared_ptr<IStorage_Write_Container>& /*parent*/)
  {
    return std::vector<std::shared_ptr<IStorage_Write_Container>>();
  }

  template <class PROD, class... PRODS>
  inline std::vector<std::shared_ptr<IStorage_Write_Container>> doWrite(
    std::shared_ptr<IStorage_File>& file,
    int const technology,
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
  inline void write(int const technology, PRODS&... prods)
  {
    auto file = createFile(technology, testFileName.c_str(), 'o');
    auto parent = createWriteAssociation(technology, testTreeName);
    parent->setFile(file);
    parent->setupWrite();

    auto keepContainersAlive = doWrite(file, technology, parent, prods...);
    keepContainersAlive.back()
      ->commit(); //Elements are in reverse order of container construction, so this makes sure container owner calls commit()
  }

  template <class PROD>
  inline std::unique_ptr<PROD const> doRead(std::shared_ptr<IStorage_File>& file,
                                            int const technology)
  {
    auto container = createReadContainer(technology, makeTestBranchName<PROD>());
    container->setFile(file);
    void const* dataPtr = new PROD();
    void const** dataPtrPtr = &dataPtr;

    if (!container->read(0, dataPtrPtr, typeid(PROD)))
      throw std::runtime_error("Failed to read a " + getTypeName<PROD>());

    return std::unique_ptr<PROD const>(static_cast<PROD const*>(dataPtr));
  }

  template <class... PRODS>
  inline std::tuple<std::unique_ptr<PRODS const>...> read(int const technology)
  {
    auto file = createFile(technology, testFileName, 'i');

    return std::make_tuple(doRead<PRODS>(file, technology)...);
  }

  inline int getTechnology(std::string const& tech_string)
  {
    std::unordered_map<std::string_view, int> const tech_lookup = {
      {"ROOT_TTREE", form::technology::ROOT_TTREE},
      {"ROOT_RNTUPLE", form::technology::ROOT_RNTUPLE},
      {"HDF5", form::technology::HDF5}};

    auto const it = tech_lookup.find(tech_string);
    if (it == tech_lookup.end()) {
      throw std::runtime_error("Unknown technology: " + tech_string);
    }

    return it->second;
  }

} // namespace form::test

#endif // TEST_FORM_TEST_UTILS_HPP
