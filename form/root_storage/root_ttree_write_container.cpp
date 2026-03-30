// Copyright (C) 2025 ...

#include "root_ttree_write_container.hpp"
#include "root_tfile.hpp"

#include "TFile.h"
#include "TTree.h"

#include <iostream> //TODO: Remove for the real PR!

using namespace form::detail::experimental;

ROOT_TTree_Write_ContainerImp::ROOT_TTree_Write_ContainerImp(std::string const& name) :
  Storage_Write_Association(name), m_tfile(nullptr), m_tree(nullptr)
{
}

ROOT_TTree_Write_ContainerImp::~ROOT_TTree_Write_ContainerImp()
{
  if (m_tree != nullptr) {
    //TODO: Remove cout for the real PR!
    std::cout << "Writing tree " << m_tree->GetName() << " to a TFile named " << m_tfile->GetName() << std::endl;
    std::cout << "Tree thinks it belongs to a file named " << m_tree->GetDirectory()->GetName() << std::endl;
    std::cout << "Global TFile is named " << gDirectory->GetName() << std::endl;
    m_tree->GetDirectory()->WriteTObject(m_tree);
    //m_tree->Write() is not good enough because that writes to the _current_ gDirectory.
    //Sometimes gDirectory is getting reset even while m_tfile is still open!  We think
    //it may have to do with multithreading.
    //I'm pretty sure we had a reason why the TFile destructor isn't good enough,
    //but I don't remember what it is right now.  Maybe recovering partial jobs?
    delete m_tree;
    std::cout << "Done with TTree container destructor" << std::endl;
  }
}

void ROOT_TTree_Write_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
{
  this->Storage_Write_Association::setFile(file);
  ROOT_TFileImp* root_tfile_imp = dynamic_cast<ROOT_TFileImp*>(file.get());
  if (root_tfile_imp == nullptr) {
    throw std::runtime_error(
      "ROOT_TTree_Write_ContainerImp::setFile can't attach to non-ROOT file");
  }
  m_tfile = dynamic_cast<ROOT_TFileImp*>(file.get())->getTFile();
  return;
}

void ROOT_TTree_Write_ContainerImp::setupWrite(std::type_info const& /* type*/)
{
  if (m_tfile == nullptr) {
    throw std::runtime_error("ROOT_TTree_Write_ContainerImp::setupWrite no file attached");
  }
  if (m_tree == nullptr) {
    m_tree = m_tfile->Get<TTree>(name().c_str());
  }
  if (m_tree == nullptr) {
    m_tree = new TTree(name().c_str(), name().c_str());
    m_tree->SetDirectory(m_tfile.get()); //I think this is necessary so other FORM containers for this tree can discover it.  But shouldn't Storage take care of that?
  }
  if (m_tree == nullptr) {
    throw std::runtime_error("ROOT_TTree_Write_ContainerImp::setupWrite no tree created");
  }
  return;
}

void ROOT_TTree_Write_ContainerImp::fill(void const* /* data*/)
{
  throw std::runtime_error("ROOT_TTree_Write_ContainerImp::fill not implemented");
}

void ROOT_TTree_Write_ContainerImp::commit()
{
  throw std::runtime_error("ROOT_TTree_Write_ContainerImp::commit not implemented");
}

TTree* ROOT_TTree_Write_ContainerImp::getTTree() { return m_tree; }
