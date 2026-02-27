//A ROOT_RField_Read_Container reads data products of a single type from vectors stored in an RNTuple field on disk.

#include "root_rfield_read_container.hpp"
#include "root_rntuple_read_container.hpp"
#include "root_tfile.hpp"
#include "demangle_name.hpp"

#include "ROOT/RNTupleReader.hxx"
#include "ROOT/RNTupleView.hxx"
#include "TFile.h"

#include <exception>

namespace form::detail::experimental {
  ROOT_RField_Read_ContainerImp::ROOT_RField_Read_ContainerImp(std::string const& name) :
    Storage_Associative_Read_Container(name), m_force_streamer_field(false)
  {
  }

  ROOT_RField_Read_ContainerImp::~ROOT_RField_Read_ContainerImp() {}

  void ROOT_RField_Read_ContainerImp::setAttribute(std::string const& key, std::string const& /*value*/)
  {
    if (key == "force_streamer_field") {
      m_force_streamer_field = true;
    } else {
      throw std::runtime_error("ROOT_RField_Read_ContainerImp supports some attributes, but not " + key);
    }
  }

  void ROOT_RField_Read_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
  {
    Storage_Read_Container::setFile(file);

    auto form_root_file = dynamic_cast<ROOT_TFileImp*>(file.get());
    if (form_root_file) {
      m_tfile = form_root_file->getTFile();
    } else {
      throw std::runtime_error(
        "ROOT_RField_Read_ContainerImp::setFile failed to convert an IStorage_File to a ROOT_TFileImp.  "
        "ROOT_RField_Read_ContainerImp only works with TFiles.");
    }

    if (!m_tfile) {
      throw std::runtime_error(
        "ROOT_RField_Read_ContainerImp::setFile failed to get a TFile from a ROOT_TFileImp");
    }

    return;
  }

  void ROOT_RField_Read_ContainerImp::setParent(std::shared_ptr<IStorage_Read_Container> parent)
  {
    this->Storage_Associative_Read_Container::setParent(parent);
  }

  bool ROOT_RField_Read_ContainerImp::read(int id, void const** data, std::type_info const& type)
  {
    //Connect to file at the last possible moment at the cost of a little run-time branching
    if (!m_view) {
      if (!m_reader) { //First time this RNTuple is read this job
        if (!m_tfile) {
          throw std::runtime_error(
            "ROOT_RField_Read_ContainerImp::read No file loaded to read from on first read() call!");
        }

        m_reader = ROOT::RNTupleReader::Open(top_name(), m_tfile->GetName());
      }

      try {
        m_view =
          std::make_unique<ROOT::RNTupleView<void>>(m_reader->GetView(col_name(), nullptr, type));
      } catch (const ROOT::RException& e) {
        //RNTupleView<void> will fail to create a field for fields written in streamer mode or for which type does not match the field's type on disk.  Passing an empty string for type forces it to create the same type of field as the object on disk.  Do this to handle streamer fields, then perform our own type check.
        m_view =
          std::make_unique<ROOT::RNTupleView<void>>(m_reader->GetView(col_name(), nullptr, ""));
        //TClass takes the "std::" off of "std::vector<>" when RNTuple's on-disk format doesn't.  Convert RNTuple's type name to match TClass for manual type check because our dictionary of choice will likely be the same as TClass.
        if (strcmp(TClass::GetClass(m_view->GetField().GetTypeName().c_str())->GetName(),
                   TClass::GetClass(type)->GetName())) {
          throw std::runtime_error(
            "ROOT_RField_containerImp::read type " + DemangleName(type) + " requested for a field named " +
            col_name() +
            " does not match the type in the file: " + m_view->GetField().GetTypeName());
        }
      }
    }

    if (id >= (int)m_reader->GetNEntries())
      return false;

    //Using RNTupleView<> to read instead of reusing REntry gives us full schema evolution support: the ROOT feature that lets us read files with an old class version into a new class version's memory.
    auto buffer = m_view->GetField().CreateObject<void>(); //PHLEX gets ownership of this memory
    if (!buffer) {
      throw std::runtime_error("ROOT_RField_Read_Container::read failed to create an object of type " +
                               m_view->GetField().GetTypeName() +
                               ".  Maybe the type name for this read() (" + DemangleName(type) +
                               ") doesn't match the type from the first read() (" +
                               m_view->GetField().GetTypeName() + ")?");
    }

    m_view->BindRawPtr(buffer.get());
    try {
      (*m_view)(id);
    } catch (const ROOT::RException& e) {
      throw std::runtime_error("ROOT_RField_Read_ContainerImp::read got a ROOT exception: " +
                               std::string(e.what()));
    }
    *data = buffer.release();

    return true;
  }
}
