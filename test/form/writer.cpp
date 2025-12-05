// Copyright (C) 2025 ...

#include "data_products/track_start.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"
#include "mock_phlex/phlex_toy_config.hpp"
#include "mock_phlex/phlex_toy_core.hpp" // toy of phlex core components
#include "toy_tracker.hpp"

#include <cstdlib>  // For rand() and srand()
#include <iostream> // For cout
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

  std::shared_ptr<mock_phlex::product_type_names> type_map = mock_phlex::createTypeMap();

  // TODO: Read configuration from config file instead of hardcoding
  // Should be: phlex::config::parse_config config = phlex::config::loadFromFile("phlex_config.json");
  // Create configuration and pass to form
  mock_phlex::config::parse_config config;
  config.addItem("trackStart", filename, form::technology::ROOT_TTREE);
  config.addItem("trackNumberHits", filename, form::technology::ROOT_TTREE);
  config.addItem("trackStartPoints", filename, form::technology::ROOT_TTREE);
  config.addItem("trackStartX", filename, form::technology::ROOT_TTREE);
  config.addContainerSetting(form::technology::ROOT_TTREE, "trackStart", "auto_flush", "1");
  config.addFileSetting(form::technology::ROOT_TTREE, filename, "compression", "kZSTD");
  config.addContainerSetting(
    form::technology::ROOT_RNTUPLE, "Toy_Tracker/trackStartPoints", "force_streamer_field", "true");

  form::experimental::form_interface form(type_map, config);

  ToyTracker tracker(4 * 1024);

  for (int nevent = 0; nevent < NUMBER_EVENT; nevent++) {
    std::cout << "PHLEX: Write Event No. " << nevent << std::endl;

    // Processing per event / data creation
    std::vector<float> track_x;

    for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {
      // phlex Alg per segment
      // Processing per sub-event
      std::vector<float> track_start_x;
      generate(track_start_x, 4 * 1024 /* * 1024*/); // sub-event processing
      float check = 0.0;
      for (float val : track_start_x)
        check += val;

      // done, phlex call write(mock_phlex::product_base)
      // sub-event writing called by phlex
      char seg_id_text[64];
      snprintf(seg_id_text, 64, seg_id, nevent, nseg);
      std::vector<mock_phlex::product_base> batch;
      std::string const creator = "Toy_Tracker";
      mock_phlex::product_base pb = {
        "trackStart", seg_id_text, &track_start_x, std::type_index{typeid(std::vector<float>)}};
      type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
      batch.push_back(pb);

      // Now write an int vector for the same event/data grain, and the same algorithm
      std::vector<int> track_n_hits;
      for (int i = 0; i < 100; ++i) {
        track_n_hits.push_back(i);
      }
      for (int val : track_n_hits)
        check += val;
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check = " << check << std::endl;
      mock_phlex::product_base pb_int = {
        "trackNumberHits", seg_id_text, &track_n_hits, std::type_index{typeid(std::vector<int>)}};
      type_map->names[std::type_index(typeid(std::vector<int>))] = "std::vector<int>";
      batch.push_back(pb_int);

      // Now write a vector of a user-defined class for the same event/data grain
      std::vector<TrackStart> start_points = tracker();
      TrackStart checkPoints;
      for (TrackStart const& point : start_points)
        checkPoints += point;
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", checkPoints = " << checkPoints << std::endl;
      mock_phlex::product_base pb_points = {"trackStartPoints",
                                            seg_id_text,
                                            &start_points,
                                            std::type_index{typeid(std::vector<TrackStart>)}};
      type_map->names[std::type_index(typeid(std::vector<TrackStart>))] = "std::vector<TrackStart>";
      batch.push_back(pb_points);

      form.write(creator, batch); // writes all data products for only this segment

      // Accumulate Data
      track_x.insert(track_x.end(), track_start_x.begin(), track_start_x.end());
    }

    std::cout << "PHLEX: Write Event segments done " << nevent << std::endl;

    float check = 0.0;
    for (float val : track_x)
      check += val;

    // event writing, current framework, will also write references
    char evt_id_text[64];
    snprintf(evt_id_text, 64, evt_id, nevent);
    std::string const creator = "Toy_Tracker_Event";
    mock_phlex::product_base pb = {
      "trackStartX", evt_id_text, &track_x, std::type_index{typeid(std::vector<float>)}};
    type_map->names[std::type_index(typeid(std::vector<float>))] = "std::vector<float>";
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << std::endl;
    form.write(creator, pb);

    std::cout << "PHLEX: Write Event done " << nevent << std::endl;
  }

  std::cout << "PHLEX: Write done " << std::endl;
  return 0;
}
