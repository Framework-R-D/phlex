// Copyright (C) 2025 ...

#include "form/form.hpp"
#include "form/phlex_toy_core.hpp" // toy of phlex core components

#include <iostream> // For cout
#include <vector>

static const int NUMBER_EVENT = 2;
static const int NUMBER_SEGMENT = 15;

static const char* const evt_id = "[EVENT=%08X]";
static const char* const seg_id = "[EVENT=%08X;SEG=%08X]";

int main(int /* argc*/, char** /* argv[]*/)
{
  std::cout << "In main" << std::endl;

  std::shared_ptr<phlex::product_type_names> type_map = phlex::createTypeMap();
  form::experimental::form_interface form(type_map);

  for (int nevent = 0; nevent < NUMBER_EVENT; nevent++) {
    std::cout << "PHLEX: Read Event No. " << nevent << std::endl;

    // Processing per event / data creation
    const std::vector<float>* vrand = nullptr;

    for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {
      // phlex Alg per segment
      // Processing per sub-event
      const std::vector<float>* vrand_seg = nullptr;
      char seg_id_text[64];
      sprintf(seg_id_text, seg_id, nevent, nseg);
      phlex::product_base pb = {"<ToyAlg_Segment>/<ToyProduct>",
                                seg_id_text,
                                vrand_seg,
                                std::type_index{typeid(std::vector<float>)}};
      type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
      form.read(pb);
      vrand_seg =
        static_cast<const std::vector<float>*>(pb.data); //FIXME: Can this be done by FORM?
      float check = 0.0;
      for (unsigned int j = 0; j < vrand_seg->size(); ++j) {
        check += vrand_seg->at(j);
      }
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check = " << check << std::endl;

      delete static_cast<const std::vector<float>*>(pb.data); //FIXME: PHLEX owns this memory!
    }
    std::cout << "PHLEX: Read Event segments done " << nevent << std::endl;

    char evt_id_text[64];
    sprintf(evt_id_text, evt_id, nevent);
    phlex::product_base pb = {"<ToyAlg_Event>/<ToyProduct>",
                              evt_id_text,
                              vrand,
                              std::type_index{typeid(std::vector<float>)}};
    type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
    form.read(pb);
    vrand = static_cast<const std::vector<float>*>(pb.data); //FIXME: Can this be done by FORM?
    float check = 0.0;
    for (unsigned int j = 0; j < vrand->size(); ++j) {
      check += vrand->at(j);
    }
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << std::endl;
    delete static_cast<const std::vector<float>*>(pb.data); //FIXME: PHLEX owns this memory!

    std::cout << "PHLEX: Read Event done " << nevent << std::endl;
  }

  std::cout << "PHLEX: Read done " << std::endl;
  return 0;
}
