// Copyright (C) 2025 ...

#include "form/form.hpp"
#include "form/phlex_toy_core.hpp" // toy of phlex core components

#include <cstdlib>  // For rand() and srand()
#include <iostream> // For cout
#include <vector>

static const int NUMBER_EVENT = 2;
static const int NUMBER_SEGMENT = 15;

static const char* const evt_id = "[EVENT=%08X]";
static const char* const seg_id = "[EVENT=%08X;SEG=%08X]";

void generate(std::vector<float>& vrand, int size)
{
  int rand1 = rand() % 32768;
  int rand2 = rand() % 32768;
  int npx = (rand1 * 32768 + rand2) % size;
  for (int nelement = 0; nelement < npx; ++nelement) {
    int rand1 = rand() % 32768;
    int rand2 = rand() % 32768;
    float random = float(rand1 * 32768 + rand2) / (32768 * 32768);
    vrand.push_back(random);
  }
}

int main(int /*argc*/, char** /* argv[]*/)
{
  std::cout << "In main" << std::endl;
  srand(time(0));

  std::shared_ptr<phlex::product_type_names> type_map = phlex::createTypeMap();
  form::experimental::form_interface form(type_map);

  for (int nevent = 0; nevent < NUMBER_EVENT; nevent++) {
    std::cout << "PHLEX: Write Event No. " << nevent << std::endl;

    // Processing per event / data creation
    std::vector<float> vrand;

    for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {
      // phlex Alg per segment
      // Processing per sub-event
      std::vector<float> vrand_seg;
      generate(vrand_seg, 4 * 1024 /* * 1024*/); // sub-event processing
      float check = 0.0;
      for (unsigned int j = 0; j < vrand_seg.size(); ++j) {
        check += vrand_seg.at(j);
      }
      // done, phlex call write(phlex::product_base)
      // sub-event writing called by phlex
      char seg_id_text[64];
      sprintf(seg_id_text, seg_id, nevent, nseg);
      std::vector<phlex::product_base> batch;
      phlex::product_base pb = {"<ToyAlg_Segment>/<ToyProduct>",
                                seg_id_text,
                                &vrand_seg,
                                std::type_index{typeid(std::vector<float>)}};
      type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check = " << check << std::endl;
      batch.push_back(pb);

      // Now write an int vector for the same event/data grain, and the same algorithm
      std::vector<int> vint;
      for (int i = 0; i < 100; ++i) {
        vint.push_back(i);
      }
      phlex::product_base pb_int = {"<ToyAlg_Segment>/<ToyProduct_i>",
                                    seg_id_text,
                                    &vint,
                                    std::type_index{typeid(std::vector<int>)}};
      type_map->names[std::type_index(typeid(std::vector<int>))] = "std::vector<int>";
      batch.push_back(pb_int);
      form.write(batch); // writes all data products for only this segment

      // Accumulate Data
      vrand.insert(vrand.end(), vrand_seg.begin(), vrand_seg.end());
    }

    std::cout << "PHLEX: Write Event segments done " << nevent << std::endl;

    float check = 0.0;
    for (unsigned int j = 0; j < vrand.size(); ++j) {
      check += vrand.at(j);
    }

    // event writing, current framework, will also write references
    char evt_id_text[64];
    sprintf(evt_id_text, evt_id, nevent);
    phlex::product_base pb = {"<ToyAlg_Event>/<ToyProduct>",
                              evt_id_text,
                              &vrand,
                              std::type_index{typeid(std::vector<float>)}};
    type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << std::endl;
    form.write(pb);

    std::cout << "PHLEX: Write Event done " << nevent << std::endl;
  }

  std::cout << "PHLEX: Write done " << std::endl;
  return 0;
}
