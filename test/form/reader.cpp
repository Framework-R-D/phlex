// Copyright (C) 2025 ...

#include "core/technology.hpp"
#include "data_products/track_start.hpp"
#include "form/form_reader.hpp"
#include "test_helpers.hpp"
#include "test_utils.hpp"

#include <cmath>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

static int const number_event = 4;
static int const number_segment = 15;

static float const tolerance = 1e-3f;

// Structs to hold expected checksums
struct seg_checksum {
  float check;
  float cpx, cpy, cpz;
};

struct evt_checksum {
  float check;
};

int main(int argc, char** argv)
{
  std::cout << "In main" << '\n';

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";
  std::string const checksum_filename = (argc > 2) ? argv[2] : "toy_checksums.txt";
  auto const technology = form::test::get_technology((argc > 3) ? argv[3] : "ROOT_TTREE");

  // Load expected checksums from file
  std::map<std::pair<int, int>, seg_checksum> expected_seg;
  std::map<int, evt_checksum> expected_evt;

  std::ifstream checksum_file(checksum_filename);
  if (!checksum_file.is_open()) {
    std::cerr << "ERROR: Could not open checksum file: " << checksum_filename << '\n';
    return 1;
  }

  std::string line;
  while (std::getline(checksum_file, line)) {
    std::istringstream iss(line);
    std::string type;
    iss >> type;
    if (type == "SEG") {
      seg_checksum cs{};
      int nevent{};
      int nseg{};
      iss >> nevent >> nseg >> cs.check >> cs.cpx >> cs.cpy >> cs.cpz;
      expected_seg[{nevent, nseg}] = cs;
    } else if (type == "EVT") {
      evt_checksum cs{};
      int nevent{};
      iss >> nevent >> cs.check;
      expected_evt[nevent] = cs;
    }
  }
  checksum_file.close();

  // TODO: Read configuration from config file instead of hardcoding
  form::experimental::config::item_config config_items;
  config_items.add_item("trackStart", filename, technology);
  config_items.add_item("trackNumberHits", filename, technology);
  config_items.add_item("trackStartPoints", filename, technology);
  config_items.add_item("trackStartX", filename, technology);

  form::experimental::config::tech_setting_config tech_config;

  form::experimental::form_reader_interface form(config_items, tech_config);

  bool all_passed = true;

  for (int nevent = 0; nevent < number_event; nevent++) {
    std::cout << "PHLEX: Read Event No. " << nevent << '\n';

    std::unique_ptr<std::vector<float> const> track_x;

    for (int nseg = 0; nseg < number_segment; nseg++) {

      void const* raw_ptr = nullptr;
      // Must match the canonical format emitted by writer.cpp (phlex data_cell_index format)
      std::string const seg_id_text = std::format("[event:{}, segment:{}]", nevent, nseg);

      std::string const& segment_id = seg_id_text;

      std::string const creator = "Toy_Tracker";

      form::experimental::product_with_name pb = {
        .label = "trackStart", .data = raw_ptr, .type = &typeid(std::vector<float>)};

      form.read(creator, segment_id, pb);
      std::unique_ptr<std::vector<float> const> track_start_x(
        static_cast<std::vector<float> const*>(pb.data));

      raw_ptr = nullptr;
      form::experimental::product_with_name pb_int = {
        .label = "trackNumberHits", .data = raw_ptr, .type = &typeid(std::vector<int>)};

      form.read(creator, segment_id, pb_int);
      std::unique_ptr<std::vector<int> const> track_n_hits(
        static_cast<std::vector<int> const*>(pb_int.data));

      raw_ptr = nullptr;
      form::experimental::product_with_name pb_points = {
        .label = "trackStartPoints", .data = raw_ptr, .type = &typeid(std::vector<track_start>)};

      form.read(creator, segment_id, pb_points);
      std::unique_ptr<std::vector<track_start> const> start_points(
        static_cast<std::vector<track_start> const*>(pb_points.data));

      float check = 0.0;
      for (float val : *track_start_x) {
        check += val;
      }
      for (int val : *track_n_hits) {
        check += static_cast<float>(val);
      }
      track_start check_points;
      for (track_start val : *start_points) {
        check_points += val;
      }
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check = " << check << '\n';
      std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                << ", check_points = " << check_points << '\n';

      // Verify segment checksums
      auto key = std::make_pair(nevent, nseg);
      if (expected_seg.contains(key)) {
        auto const& exp = expected_seg[key];
        bool seg_ok = (std::fabs(check - exp.check) <= tolerance) &&
                      (std::fabs(check_points.get_x() - exp.cpx) <= tolerance) &&
                      (std::fabs(check_points.get_y() - exp.cpy) <= tolerance) &&
                      (std::fabs(check_points.get_z() - exp.cpz) <= tolerance);
        if (seg_ok) {
          std::cout << "VERIFY PASS: event=" << nevent << " seg=" << nseg << '\n';
        } else {
          std::cerr << "VERIFY FAIL: event=" << nevent << " seg=" << nseg
                    << " expected check=" << exp.check << " got=" << check
                    << " expected cpx=" << exp.cpx << " got=" << check_points.get_x()
                    << " expected cpy=" << exp.cpy << " got=" << check_points.get_y()
                    << " expected cpz=" << exp.cpz << " got=" << check_points.get_z() << '\n';
          all_passed = false;
        }
      } else {
        std::cerr << "VERIFY FAIL: no expected checksum for event=" << nevent << " seg=" << nseg
                  << '\n';
        all_passed = false;
      }
    }
    std::cout << "PHLEX: Read Event segments done " << nevent << '\n';

    std::string const evt_id_text = std::format("[event:{}]", nevent);

    std::string const& event_id = evt_id_text;

    std::string const creator = "Toy_Tracker_Event";

    void const* raw_evt_ptr = nullptr;
    form::experimental::product_with_name pb = {
      .label = "trackStartX", .data = raw_evt_ptr, .type = &typeid(std::vector<float>)};

    form.read(creator, event_id, pb);
    track_x.reset(static_cast<std::vector<float> const*>(pb.data));

    float check = 0.0;
    for (float val : *track_x) {
      check += val;
    }
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << '\n';

    // Verify event checksum
    if (expected_evt.contains(nevent)) {
      auto const& exp = expected_evt[nevent];
      bool evt_ok = (std::fabs(check - exp.check) <= tolerance);
      if (evt_ok) {
        std::cout << "VERIFY PASS: event=" << nevent << '\n';
      } else {
        std::cerr << "VERIFY FAIL: event=" << nevent << " expected check=" << exp.check
                  << " got=" << check << '\n';
        all_passed = false;
      }
    } else {
      std::cerr << "VERIFY FAIL: no expected checksum for event=" << nevent << '\n';
      all_passed = false;
    }

    std::cout << "PHLEX: Read Event done " << nevent << '\n';
  }

  // Report overall result
  if (all_passed) {
    std::cout << "PHLEX: All verification checks PASSED." << '\n';
    return 0;
  }
  std::cerr << "PHLEX: Some verification checks FAILED." << '\n';
  return 1;
}
