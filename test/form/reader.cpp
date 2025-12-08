// Copyright (C) 2025 ...

#include "data_products/track_start.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"
#include "mock_phlex/phlex_toy_config.hpp"
#include "mock_phlex/phlex_toy_core.hpp" // toy of phlex core components

#include <iostream> // For cout
#include <vector>

static int const NUMBER_EVENT = 4;
static int const NUMBER_SEGMENT = 15;

static char const* const evt_id = "[EVENT=%08X]";
static char const* const seg_id = "[EVENT=%08X;SEG=%08X]";

int main(int argc, char** argv)
{
  std::cout << "In main" << std::endl;

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";

  std::shared_ptr<mock_phlex::product_type_names> type_map = mock_phlex::createTypeMap();

  // TODO: Read configuration from config file instead of hardcoding
  // Should be: phlex::config::parse_config config = phlex::config::loadFromFile("phlex_config.json");
  // Create configuration and pass to form
  mock_phlex::config::parse_config config;
  config.addItem("trackStart", filename, form::technology::ROOT_TTREE);
  config.addItem("trackNumberHits", filename, form::technology::ROOT_TTREE);
  config.addItem("trackStartPoints", filename, form::technology::ROOT_TTREE);
  config.addItem("trackStartX", filename, form::technology::ROOT_TTREE);

  form::experimental::form_interface form(type_map, config);

  for (int nevent = 0; nevent < NUMBER_EVENT; nevent++) {
    std::cout << "PHLEX: Read Event No. " << nevent << std::endl;

    // Processing per event / data creation
    std::vector<float> const* track_x = nullptr;

    for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {
      // phlex Alg per segment
      // Processing per sub-event
      std::vector<float> const* track_start_x = nullptr;
      char seg_id_text[64];
      snprintf(seg_id_text, 64, seg_id, nevent, nseg);
      std::string const creator = "Toy_Tracker";
      mock_phlex::product_base pb = {
        "trackStart", seg_id_text, track_start_x, std::type_index{typeid(std::vector<float>)}};
      type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
      form.read(creator, pb);
      track_start_x =
        static_cast<std::vector<float> const*>(pb.data); //FIXME: Can this be done by FORM?
      std::vector<int> const* track_n_hits = nullptr;
      mock_phlex::product_base pb_int = {
        "trackNumberHits", seg_id_text, track_n_hits, std::type_index{typeid(std::vector<int>)}};
      type_map->names[std::type_index(typeid(std::vector<int>))] = "std::vector<int>";
      form.read(creator, pb_int);
      track_n_hits = static_cast<std::vector<int> const*>(pb_int.data);

      std::vector<TrackStart> const* start_points = nullptr;
      mock_phlex::product_base pb_points = {"trackStartPoints",
                                            seg_id_text,
                                            start_points,
                                            std::type_index{typeid(std::vector<TrackStart>)}};
      type_map->names[std::type_index(typeid(std::vector<TrackStart>))] = "std::vector<TrackStart>";
      form.read(creator, pb_points);
      start_points = static_cast<std::vector<TrackStart> const*>(pb_points.data);

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
    snprintf(evt_id_text, 64, evt_id, nevent);
    std::string const creator = "Toy_Tracker_Event";
    mock_phlex::product_base pb = {
      "trackStartX", evt_id_text, track_x, std::type_index{typeid(std::vector<float>)}};
    type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
    form.read(creator, pb);
    track_x = static_cast<std::vector<float> const*>(pb.data); //FIXME: Can this be done by FORM?
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
