// Copyright (C) 2025 ...

#include "core/technology.hpp"
#include "data_products/track_start.hpp"
#include "form/form_writer.hpp"
#include "test_helpers.hpp"
#include "test_utils.hpp"
#include "toy_tracker.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <ranges>
#include <vector>

static int const number_event = 4;
static int const number_segment = 15;

struct generator {
  generator() : gen_(std::chrono::system_clock::now().time_since_epoch().count()), dist_(0, 1) {}

  void operator()(std::vector<float>& vrand, int size)
  {
    assert(size > 1);
    std::uniform_int_distribution size_dist(0, size - 1);
    size_t const how_many = size_dist(gen_);
    vrand.resize(how_many);

    for (auto& rand : vrand) {
      rand = dist_(gen_);
    }
  }

private:
  std::mt19937 gen_;
  std::uniform_real_distribution<float> dist_;
};

int main(int argc, char** argv)
{
  std::cout << "In main" << '\n';

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";
  std::string const checksum_filename = (argc > 2) ? argv[2] : "toy_checksums.txt";
  auto const technology = form::test::get_technology((argc > 3) ? argv[3] : "ROOT_TTREE");

  // TODO: Read configuration from config file instead of hardcoding
  form::experimental::config::item_config config_items;
  config_items.add_item("trackStart", filename, technology);
  config_items.add_item("trackNumberHits", filename, technology);
  config_items.add_item("trackStartPoints", filename, technology);
  config_items.add_item("trackStartX", filename, technology);

  form::experimental::config::tech_setting_config tech_config;
  tech_config.container_settings[form::technology::root_ttree]["trackStart"].emplace_back(
    "auto_flush", "1");
  tech_config.file_settings[technology]["toy.root"].emplace_back("compression", "kZSTD");
  tech_config.container_settings[form::technology::root_rntuple]["Toy_Tracker/trackStartPoints"]
    .emplace_back("force_streamer_field", "true");

  form::experimental::form_writer_interface form(config_items, tech_config);

  toy_tracker tracker(4 * 1024);

  // Open checksum file for writing
  std::ofstream checksum_file(checksum_filename);
  if (!checksum_file.is_open()) {
    std::cerr << "ERROR: Could not open checksum file: " << checksum_filename << '\n';
    return 1;
  }

  generator generate;

  for (int nevent = 0; nevent < number_event; nevent++) {
    std::cout << "PHLEX: Write Event No. " << nevent << '\n';

    std::vector<float> track_x;

    for (int nseg = 0; nseg < number_segment; nseg++) {

      std::vector<float> track_start_x;
      generate(track_start_x, 4 * 1024 /* * 1024*/); // sub-event processing
      float check = 0.0;
      for (float val : track_start_x) {
        check += val;
      }

      // Canonical Phlex index format: [layer:number, ...], base-10, ", "-joined.
      // Matches phlex::data_cell_index::to_string() and is directly parseable by the source parser.
      std::string const seg_id_text = std::format("[event:{}, segment:{}]", nevent, nseg);

      std::string const& segment_id = seg_id_text;

      std::vector<form::experimental::product_with_name> products;
      std::string const creator = "Toy_Tracker";

      form::experimental::product_with_name pb = {
        .label = "trackStart", .data = &track_start_x, .type = &typeid(std::vector<float>)};
      products.push_back(pb);

      std::vector<int> track_n_hits(std::from_range, std::views::iota(0, 100));
      for (int val : track_n_hits) {
        check += static_cast<float>(val);
      }
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check = " << check << '\n';

      form::experimental::product_with_name pb_int = {
        .label = "trackNumberHits", .data = &track_n_hits, .type = &typeid(std::vector<int>)};
      products.push_back(pb_int);

      std::vector<track_start> start_points = tracker();
      track_start check_points;
      for (track_start const& point : start_points) {
        check_points += point;
      }
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check_points = " << check_points << '\n';

      form::experimental::product_with_name pb_points = {.label = "trackStartPoints",
                                                         .data = &start_points,
                                                         .type = &typeid(std::vector<track_start>)};
      products.push_back(pb_points);

      form.write(creator, segment_id, products);

      // Save segment checksums
      checksum_file << std::setprecision(10) << "SEG " << nevent << " " << nseg << " " << check
                    << " " << check_points.get_x() << " " << check_points.get_y() << " "
                    << check_points.get_z() << "\n";
      track_x.insert(track_x.end(), track_start_x.begin(), track_start_x.end());
    }

    std::cout << "PHLEX: Write Event segments done " << nevent << '\n';

    float check = 0.0;
    for (float val : track_x) {
      check += val;
    }

    std::string const evt_id_text = std::format("[event:{}]", nevent);

    std::string const& event_id = evt_id_text;

    std::string const creator = "Toy_Tracker_Event";

    form::experimental::product_with_name pb = {
      .label = "trackStartX", .data = &track_x, .type = &typeid(std::vector<float>)};
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << '\n';

    form.write(creator, event_id, pb);

    // Save event checksum
    checksum_file << std::setprecision(10) << "EVT " << nevent << " " << check << "\n";
    std::cout << "PHLEX: Write Event done " << nevent << '\n';
  }

  checksum_file.close();
  std::cout << "PHLEX: Write done. Checksums saved to " << checksum_filename << '\n';
  return 0;
}
