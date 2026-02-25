// Copyright (C) 2025 ...

#include "data_products/track_start.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"
#include "test_helpers.hpp"
#include "toy_tracker.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

static int const NUMBER_EVENT = 4;
static int const NUMBER_SEGMENT = 15;

static char const* const evt_id = "[EVENT=%08X]";
static char const* const seg_id = "[EVENT=%08X;SEG=%08X]";

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

int main(int argc, char** argv)
{
  std::cout << "In main" << std::endl;
  srand(time(0));

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";
  std::string const checksum_filename = (argc > 2) ? argv[2] : "toy_checksums.txt";

  // TODO: Read configuration from config file instead of hardcoding
  form::experimental::config::output_item_config output_config;
  output_config.addItem("trackStart", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackNumberHits", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackStartPoints", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackStartX", filename, form::technology::ROOT_TTREE);

  form::experimental::config::tech_setting_config tech_config;
  tech_config.container_settings[form::technology::ROOT_TTREE]["trackStart"].emplace_back(
    "auto_flush", "1");
  tech_config.file_settings[form::technology::ROOT_TTREE]["toy.root"].emplace_back("compression",
                                                                                   "kZSTD");
  tech_config.container_settings[form::technology::ROOT_RNTUPLE]["Toy_Tracker/trackStartPoints"]
    .emplace_back("force_streamer_field", "true");

  form::experimental::form_interface form(output_config, tech_config);

  ToyTracker tracker(4 * 1024);

  // Open checksum file for writing
  std::ofstream checksum_file(checksum_filename);
  if (!checksum_file.is_open()) {
    std::cerr << "ERROR: Could not open checksum file: " << checksum_filename << std::endl;
    return 1;
  }

  for (int nevent = 0; nevent < NUMBER_EVENT; nevent++) {
    std::cout << "PHLEX: Write Event No. " << nevent << std::endl;

    std::vector<float> track_x;

    for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {

      std::vector<float> track_start_x;
      generate(track_start_x, 4 * 1024 /* * 1024*/); // sub-event processing
      float check = 0.0;
      for (float val : track_start_x)
        check += val;

      char seg_id_text[64];
      snprintf(seg_id_text, 64, seg_id, nevent, nseg);

      std::string segment_id(seg_id_text);

      std::vector<form::experimental::product_with_name> products;
      std::string const creator = "Toy_Tracker";

      form::experimental::product_with_name pb = {
        "trackStart", &track_start_x, &typeid(std::vector<float>)};
      products.push_back(pb);

      std::vector<int> track_n_hits;
      for (int i = 0; i < 100; ++i) {
        track_n_hits.push_back(i);
      }
      for (int val : track_n_hits)
        check += val;
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check = " << check << std::endl;

      form::experimental::product_with_name pb_int = {
        "trackNumberHits", &track_n_hits, &typeid(std::vector<int>)};
      products.push_back(pb_int);

      std::vector<TrackStart> start_points = tracker();
      TrackStart checkPoints;
      for (TrackStart const& point : start_points)
        checkPoints += point;
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", checkPoints = " << checkPoints << std::endl;

      form::experimental::product_with_name pb_points = {
        "trackStartPoints", &start_points, &typeid(std::vector<TrackStart>)};
      products.push_back(pb_points);

      form.write(creator, segment_id, products);

      // Save segment checksums
      checksum_file << std::setprecision(10) << "SEG " << nevent << " " << nseg << " " << check
                    << " " << checkPoints.getX() << " " << checkPoints.getY() << " "
                    << checkPoints.getZ() << "\n";
      track_x.insert(track_x.end(), track_start_x.begin(), track_start_x.end());
    }

    std::cout << "PHLEX: Write Event segments done " << nevent << std::endl;

    float check = 0.0;
    for (float val : track_x)
      check += val;

    char evt_id_text[64];
    snprintf(evt_id_text, 64, evt_id, nevent);

    std::string event_id(evt_id_text);

    std::string const creator = "Toy_Tracker_Event";

    form::experimental::product_with_name pb = {
      "trackStartX", &track_x, &typeid(std::vector<float>)};
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << std::endl;

    form.write(creator, event_id, pb);

    // Save event checksum
    checksum_file << std::setprecision(10) << "EVT " << nevent << " " << check << "\n";
    std::cout << "PHLEX: Write Event done " << nevent << std::endl;
  }

  checksum_file.close();
  std::cout << "PHLEX: Write done. Checksums saved to " << checksum_filename << std::endl;
  return 0;
}
