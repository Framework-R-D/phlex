//A ROOT_RField_Container reads data products of a single type from vectors stored in an RNTuple field on disk.

#include "root_rfield_container.hpp"
#include "root_rntuple_container.hpp"
#include "root_tfile.hpp"

#include "ROOT/RNTupleReader.hxx"
#include "ROOT/RNTupleView.hxx"
#include "ROOT/RNTupleWriter.hxx"
#include "TFile.h"

#include <exception>
#include <iostream>

namespace form::detail::experimental {
  ROOT_RField_ContainerImp::ROOT_RField_ContainerImp(std::string const& name) :
    Storage_Associative_Container(name), m_force_streamer_field(false)
  {
  }

  ROOT_RField_ContainerImp::~ROOT_RField_ContainerImp() {}

  void ROOT_RField_ContainerImp::setAttribute(std::string const& key, std::string const& /*value*/)
  {
    if (key == "force_streamer_field") {
      m_force_streamer_field = true;
    } else {
      throw std::runtime_error("ROOT_RField_ContainerImp supports some attributes, but not " + key);
    }
  }

  void ROOT_RField_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
  {
    Storage_Container::setFile(file);
    m_tfile = dynamic_cast<ROOT_TFileImp*>(file.get())->getTFile();
    return;
  }

  void ROOT_RField_ContainerImp::setParent(std::shared_ptr<IStorage_Container> parent)
  {
    this->Storage_Associative_Container::setParent(parent);
    auto parentDerived = dynamic_pointer_cast<ROOT_RNTuple_ContainerImp>(parent);
    if (!parentDerived) {
      throw std::runtime_error(
        "ROOT_RField_ContainerImp::setParent parent is not a ROOT_RNTuple_ContainerImp!  Something "
        "may be wrong with how Storage works.");
    }
    m_rntuple_parent = parentDerived;
  }

  bool ROOT_RField_ContainerImp::read(int id, void const** data, std::string& type)
  {
    //Connect to file at the last possible moment at the cost of a little run-time branching
    if (!m_view) {
      if (!m_reader) { //First time this RNTuple is read this job
        if (!m_tfile) {
          throw std::runtime_error(
            "ROOT_RField_ContainerImp::read No file loaded to read from on first read() call!");
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
                   type.c_str())) {
          throw std::runtime_error(
            "ROOT_RField_containerImp::read type " + type + " requested for a field named " +
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
      throw std::runtime_error("ROOT_RField_Container::read failed to create an object of type " +
                               m_view->GetField().GetTypeName() +
                               ".  Maybe the type name for this read() (" + type +
                               ") doesn't match the type from the first read() (" +
                               m_view->GetField().GetTypeName() + ")?");
    }

    m_view->BindRawPtr(buffer.get());
    try {
      (*m_view)(id);
    } catch (const ROOT::RException& e) {
      throw std::runtime_error("ROOT_RField_ContainerImp::read got a ROOT exception: " +
                               std::string(e.what()));
    }
    *data = buffer.release();

    return true;
  }

  void ROOT_RField_ContainerImp::fill(void const* data)
  {
    if (!m_rntuple_parent->m_writer) {
      m_rntuple_parent->m_writer =
        ROOT::RNTupleWriter::Append(std::move(m_rntuple_parent->m_model), top_name(), *m_tfile);
      m_rntuple_parent->m_entry = m_rntuple_parent->m_writer->CreateRawPtrWriteEntry();
    }
    m_rntuple_parent->m_entry->BindRawPtr(col_name(), data);
  }

  void ROOT_RField_ContainerImp::commit()
  {
    if (!m_rntuple_parent->m_entry) {
      throw std::runtime_error("ROOT_RField_ContainerImp::commit No RRawPtrWriteEntry set up.  "
                               "You may have called commit() without calling setupWrite() first.");
    }
    if (!m_rntuple_parent->m_writer) {
      throw std::runtime_error("ROOT_RField_ContainerImp::commit No RNTupleWriter set up.  "
                               "You may have called commit() without calling setupWrite() first.");
    }
    m_rntuple_parent->m_writer->Fill(*m_rntuple_parent->m_entry);
  }

  void ROOT_RField_ContainerImp::setupWrite(std::string const& type)
  {
    std::unique_ptr<ROOT::RFieldBase> field;

    if (m_force_streamer_field) {
      field = std::make_unique<ROOT::RStreamerField>(col_name(), type);
    } else {
      auto result = ROOT::RFieldBase::Create(col_name(), type);
      if (result) {
        field = result.Unwrap();
      } else {
        std::cerr
          << "ROOT_RField_ContainerImp::setupWrite could not create column-wise storage for "
          << type
          << ".  This class is probably using something obsolete like TLorentzVector.  Storing it "
             "in streamer mode to keep the application going."
          << std::endl;
        field = std::make_unique<ROOT::RStreamerField>(col_name(), type);
      }
    }

    m_rntuple_parent->m_model->AddField(std::move(field));
  }
}
