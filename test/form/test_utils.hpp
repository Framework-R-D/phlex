//Utilities to make FORM unit tests easier to write and maintain

#ifndef __TEST_UTILS_HPP__
#define __TEST_UTILS_HPP__

#include "storage/istorage.hpp"
#include "storage/storage_associative_container.hpp"
#include "util/factories.hpp"

#include "TClass.h"

#include <iostream>
#include <memory>

using namespace form::detail::experimental;

namespace form::test {

  std::string const testTreeName = "FORMTestTree";
  std::string const testFileName = "FORMTestFile.root";

  template <class PROD>
  std::string getTypeName()
  {
    return TClass::GetClass<PROD>()->GetName();
  }

  template <class PROD>
  std::string makeTestBranchName()
  {
    return testTreeName + "/" + getTypeName<PROD>();
  }

  std::vector<std::shared_ptr<IStorage_Container>> doWrite(
    std::shared_ptr<IStorage_File>& /*file*/,
    int const /*technology*/,
    std::shared_ptr<IStorage_Container>& /*parent*/)
  {
    return std::vector<std::shared_ptr<IStorage_Container>>();
  }

  template <class PROD, class... PRODS>
  std::vector<std::shared_ptr<IStorage_Container>> doWrite(
    std::shared_ptr<IStorage_File>& file,
    int const technology,
    std::shared_ptr<IStorage_Container>& parent,
    PROD& prod,
    PRODS&... prods)
  {
    auto const branchName = makeTestBranchName<PROD>();
    auto container = createContainer(technology, branchName);
    auto assoc = dynamic_pointer_cast<Storage_Associative_Container>(container);
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
  void write(int const technology, PRODS&... prods)
  {
    auto file = createFile(technology, testFileName.c_str(), 'o');
    auto parent = createAssociation(technology, testTreeName);
    parent->setFile(file);
    parent->setupWrite();

    auto keepContainersAlive = doWrite(file, technology, parent, prods...);
    keepContainersAlive.back()
      ->commit(); //Elements are in reverse order of container construction, so this makes sure container owner calls commit()
  }

  template <class PROD>
  std::unique_ptr<PROD const> doRead(std::shared_ptr<IStorage_File>& file,
                               int const technology,
                               std::shared_ptr<IStorage_Container>& parent)
  {
    auto container = createContainer(technology, makeTestBranchName<PROD>());
    auto assoc = dynamic_pointer_cast<Storage_Associative_Container>(container);
    if (assoc) {
      assoc->setParent(parent);
    }
    container->setFile(file);
    void const* dataPtr = new PROD();
    void const** dataPtrPtr = &dataPtr;

    if (!container->read(0, dataPtrPtr, typeid(PROD)))
      throw std::runtime_error("Failed to read a " + getTypeName<PROD>());

    return std::unique_ptr<PROD const>(static_cast<PROD const*>(dataPtr));
  }

  template <class... PRODS>
  std::tuple<std::unique_ptr<PRODS const>...> read(int const technology)
  {
    auto file = createFile(technology, testFileName, 'i');
    auto parent = createAssociation(technology, testTreeName);
    parent->setFile(file);

    return std::make_tuple(doRead<PRODS>(file, technology, parent)...);
  }

  int getTechnology(int const argc, char const** argv)
  {
    if (argc > 2 || (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
      std::cerr << "Expected exactly one argument, but got " << argc - 1 << "\n"
                << "USAGE: testSchemaWriteOldProduct <technologyInt>\n";
      return -1;
    }

    //Default to TTree with TFile
    int technology = 256 * 1 + 1;

    if (argc == 2)
      technology = std::stoi(argv[1]);

    return technology;
  }

} // namespace form::test

#endif //__TEST_UTILS_HPP__
