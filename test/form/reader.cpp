// Copyright (C) 2025 ...

#include "data_products/track_start.hpp"
#include "form/form.hpp"
#include "form/phlex_toy_core.hpp" // toy of phlex core components

#include <iostream> // For cout
#include <vector>

static const int NUMBER_EVENT = 4;
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
    const std::vector<float>* track_x = nullptr;

    for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {
      // phlex Alg per segment
      // Processing per sub-event
      const std::vector<float>* track_start_x = nullptr;
      char seg_id_text[64];
      sprintf(seg_id_text, seg_id, nevent, nseg);
      phlex::product_base pb = {"Toy_Tracker/trackStart",
                                seg_id_text,
                                track_start_x,
                                std::type_index{typeid(std::vector<float>)}};
      type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
      form.read(pb);
      track_start_x =
        static_cast<const std::vector<float>*>(pb.data); //FIXME: Can this be done by FORM?
      const std::vector<int>* track_n_hits = nullptr;
      phlex::product_base pb_int = {"Toy_Tracker/trackNumberHits",
                                    seg_id_text,
                                    track_n_hits,
                                    std::type_index{typeid(std::vector<int>)}};
      type_map->names[std::type_index(typeid(std::vector<int>))] = "std::vector<int>";
      form.read(pb_int);
      track_n_hits = static_cast<const std::vector<int>*>(pb_int.data);

      const std::vector<TrackStart>* start_points = nullptr;
      phlex::product_base pb_points = {"Toy_Tracker/trackStartPoints",
                                       seg_id_text,
                                       start_points,
                                       std::type_index{typeid(std::vector<TrackStart>)}};
      type_map->names[std::type_index(typeid(std::vector<TrackStart>))] = "std::vector<TrackStart>";
      form.read(pb_points);
      start_points = static_cast<const std::vector<TrackStart>*>(pb_points.data);

      float check = 0.0;
      for (float val : *track_start_x)
        check += val;
      for (int val : *track_n_hits)
        check += val;
      TrackStart checkPoints;
      for (TrackStart val : *start_points)
        checkPoints += val;
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check = " << check << std::endl;
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", checkPoints = " << checkPoints << std::endl;

      delete track_start_x;
      delete track_n_hits;
      delete start_points;
    }
    std::cout << "PHLEX: Read Event segments done " << nevent << std::endl;

    char evt_id_text[64];
    sprintf(evt_id_text, evt_id, nevent);
    phlex::product_base pb = {"Toy_Tracker_Event/trackStart",
                              evt_id_text,
                              track_x,
                              std::type_index{typeid(std::vector<float>)}};
    type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
    form.read(pb);
    track_x = static_cast<const std::vector<float>*>(pb.data); //FIXME: Can this be done by FORM?
    float check = 0.0;
    for (float val : *track_x)
      check += val;
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << std::endl;
    delete track_x; //FIXME: PHLEX owns this memory!

    std::cout << "PHLEX: Read Event done " << nevent << std::endl;
  }

  std::cout << "PHLEX: Read done " << std::endl;
  return 0;
}
