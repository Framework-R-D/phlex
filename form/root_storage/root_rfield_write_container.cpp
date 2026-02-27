//A ROOT_RField_Write_Container writes data products of a single type from vectors stored in an RNTuple field on disk.

#include "root_rfield_write_container.hpp"
#include "root_rntuple_write_container.hpp"
#include "root_tfile.hpp"
#include "demangle_name.hpp"

#include "ROOT/RNTupleWriter.hxx"
#include "TFile.h"

#include <exception>

namespace form::detail::experimental {
  ROOT_RField_Write_ContainerImp::ROOT_RField_Write_ContainerImp(std::string const& name) :
    Storage_Associative_Write_Container(name), m_force_streamer_field(false)
  {
  }

  ROOT_RField_Write_ContainerImp::~ROOT_RField_Write_ContainerImp() {}

  void ROOT_RField_Write_ContainerImp::setAttribute(std::string const& key, std::string const& /*value*/)
  {
    if (key == "force_streamer_field") {
      m_force_streamer_field = true;
    } else {
      throw std::runtime_error("ROOT_RField_Write_ContainerImp supports some attributes, but not " + key);
    }
  }

  void ROOT_RField_Write_ContainerImp::setFile(std::shared_ptr<IStorage_File> file)
  {
    Storage_Write_Container::setFile(file);

    auto form_root_file = dynamic_cast<ROOT_TFileImp*>(file.get());
    if (form_root_file) {
      m_tfile = form_root_file->getTFile();
    } else {
      throw std::runtime_error(
        "ROOT_RField_Write_ContainerImp::setFile failed to convert an IStorage_File to a ROOT_TFileImp.  "
        "ROOT_RField_Write_ContainerImp only works with TFiles.");
    }

    if (!m_tfile) {
      throw std::runtime_error(
        "ROOT_RField_Write_ContainerImp::setFile failed to get a TFile from a ROOT_TFileImp");
    }

    return;
  }

  void ROOT_RField_Write_ContainerImp::setParent(std::shared_ptr<IStorage_Write_Container> parent)
  {
    this->Storage_Associative_Write_Container::setParent(parent);
    auto parentDerived = dynamic_pointer_cast<ROOT_RNTuple_Write_ContainerImp>(parent);
    if (!parentDerived) {
      throw std::runtime_error(
        "ROOT_RField_Write_ContainerImp::setParent parent is not a ROOT_RNTuple_Write_ContainerImp!  Something "
        "may be wrong with how Storage works.");
    }
    m_rntuple_parent = parentDerived;
  }

  void ROOT_RField_Write_ContainerImp::fill(void const* data)
  {
    if (!m_rntuple_parent->m_writer) {
      if (!m_tfile) {
        throw std::runtime_error(
          "ROOT_RField_Write_ContainerImp::fill No file loaded to write to on first fill() call");
      }

      m_rntuple_parent->m_writer =
        ROOT::RNTupleWriter::Append(std::move(m_rntuple_parent->m_model), top_name(), *m_tfile);
      m_rntuple_parent->m_entry = m_rntuple_parent->m_writer->CreateRawPtrWriteEntry();
    }
    m_rntuple_parent->m_entry->BindRawPtr(col_name(), data);
  }

  void ROOT_RField_Write_ContainerImp::commit()
  {
    if (!m_rntuple_parent->m_entry) {
      throw std::runtime_error("ROOT_RField_Write_ContainerImp::commit No RRawPtrWriteEntry set up.  "
                               "You may have called commit() without calling setupWrite() first.");
    }
    if (!m_rntuple_parent->m_writer) {
      throw std::runtime_error("ROOT_RField_Write_ContainerImp::commit No RNTupleWriter set up.  "
                               "You may have called commit() without calling setupWrite() first.");
    }
    m_rntuple_parent->m_writer->Fill(*m_rntuple_parent->m_entry);
  }

  void ROOT_RField_Write_ContainerImp::setupWrite(std::type_info const& type)
  {
    auto const& type_name = DemangleName(type);
    std::unique_ptr<ROOT::RFieldBase> field;

    if (m_force_streamer_field) {
      field = std::make_unique<ROOT::RStreamerField>(col_name(), type_name);
    } else {
      auto result = ROOT::RFieldBase::Create(col_name(), type_name);
      if (result) {
        field = result.Unwrap();
      } else {
        std::cerr
          << "ROOT_RField_Write_ContainerImp::setupWrite could not create column-wise storage for "
          << type_name
          << ".  This class is probably using something obsolete like TLorentzVector.  Storing it "
             "in streamer mode to keep the application going."
          << std::endl;
        field = std::make_unique<ROOT::RStreamerField>(col_name(), type_name);
      }
    }

    m_rntuple_parent->m_model->AddField(std::move(field));
  }
}
