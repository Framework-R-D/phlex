// Copyright (C) 2025 ...

#include "data_products/track_start.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"
#include "test_helpers.hpp"

#include <iostream>
#include <vector>

static int const NUMBER_EVENT = 4;
static int const NUMBER_SEGMENT = 15;

static char const* const evt_id = "[EVENT=%08X]";
static char const* const seg_id = "[EVENT=%08X;SEG=%08X]";

int main(int argc, char** argv)
{
  std::cout << "In main" << std::endl;

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";

  std::shared_ptr<form::experimental::product_type_names> type_map =
    form::experimental::createTypeMap();

  // TODO: Read configuration from config file instead of hardcoding
  form::experimental::config::output_item_config output_config;
  output_config.addItem("trackStart", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackNumberHits", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackStartPoints", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackStartX", filename, form::technology::ROOT_TTREE);

  form::experimental::config::tech_setting_config tech_config;

  form::experimental::form_interface form(type_map, output_config, tech_config);

  for (int nevent = 0; nevent < NUMBER_EVENT; nevent++) {
    std::cout << "PHLEX: Read Event No. " << nevent << std::endl;

    std::vector<float> const* track_x = nullptr;

    for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {

      std::vector<float> const* track_start_x = nullptr;
      char seg_id_text[64];
      snprintf(seg_id_text, 64, seg_id, nevent, nseg);

      std::string segment_id(seg_id_text);

      std::string const creator = "Toy_Tracker";

      form::experimental::product_with_name pb = {
        "trackStart", track_start_x, &typeid(std::vector<float>)};
      type_map->names[typeid(std::vector<float>).name()] = "std::vector<float>";

      form.read(creator, segment_id, pb);
      track_start_x =
        static_cast<std::vector<float> const*>(pb.data); //FIXME: Can this be done by FORM?

      std::vector<int> const* track_n_hits = nullptr;

      form::experimental::product_with_name pb_int = {
        "trackNumberHits", track_n_hits, &typeid(std::vector<int>)};
      type_map->names[typeid(std::vector<int>).name()] = "std::vector<int>";

      form.read(creator, segment_id, pb_int);
      track_n_hits = static_cast<std::vector<int> const*>(pb_int.data);

      std::vector<TrackStart> const* start_points = nullptr;

      form::experimental::product_with_name pb_points = {
        "trackStartPoints", start_points, &typeid(std::vector<TrackStart>)};
      type_map->names[typeid(std::vector<TrackStart>).name()] = "std::vector<TrackStart>";

      form.read(creator, segment_id, pb_points);
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

    std::string event_id(evt_id_text);

    std::string const creator = "Toy_Tracker_Event";

    form::experimental::product_with_name pb = {
      "trackStartX", track_x, &typeid(std::vector<float>)};
    type_map->names[typeid(std::vector<float>).name()] = "std::vector<float>";

    form.read(creator, event_id, pb);
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
