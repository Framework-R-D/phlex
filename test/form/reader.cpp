// Copyright (C) 2025 ...

#include "data_products/track_start.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"
#include "test_helpers.hpp"

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

static int const NUMBER_EVENT = 4;
static int const NUMBER_SEGMENT = 15;

static char const* const evt_id = "[EVENT=%08X]";
static char const* const seg_id = "[EVENT=%08X;SEG=%08X]";
static float const TOLERANCE = 1e-3f;

// Structs to hold expected checksums
struct SegChecksum {
  float check;
  float cpx, cpy, cpz;
};

struct EvtChecksum {
  float check;
};

int main(int argc, char** argv)
{
  std::cout << "In main" << std::endl;

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";
  std::string const checksum_filename = (argc > 2) ? argv[2] : "toy_checksums.txt";

  // Load expected checksums from file
  std::map<std::pair<int, int>, SegChecksum> expected_seg;
  std::map<int, EvtChecksum> expected_evt;

  std::ifstream checksum_file(checksum_filename);
  if (!checksum_file.is_open()) {
    std::cerr << "ERROR: Could not open checksum file: " << checksum_filename << std::endl;
    return 1;
  }

  std::string line;
  while (std::getline(checksum_file, line)) {
    std::istringstream iss(line);
    std::string type;
    iss >> type;
    if (type == "SEG") {
      int nevent, nseg;
      SegChecksum cs;
      iss >> nevent >> nseg >> cs.check >> cs.cpx >> cs.cpy >> cs.cpz;
      expected_seg[{nevent, nseg}] = cs;
    } else if (type == "EVT") {
      int nevent;
      EvtChecksum cs;
      iss >> nevent >> cs.check;
      expected_evt[nevent] = cs;
    }
  }
  checksum_file.close();

  // TODO: Read configuration from config file instead of hardcoding
  form::experimental::config::output_item_config output_config;
  output_config.addItem("trackStart", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackNumberHits", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackStartPoints", filename, form::technology::ROOT_TTREE);
  output_config.addItem("trackStartX", filename, form::technology::ROOT_TTREE);

  form::experimental::config::tech_setting_config tech_config;

  form::experimental::form_interface form(output_config, tech_config);

  bool all_passed = true;

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

      form.read(creator, segment_id, pb);
      track_start_x =
        static_cast<std::vector<float> const*>(pb.data); //FIXME: Can this be done by FORM?

      std::vector<int> const* track_n_hits = nullptr;

      form::experimental::product_with_name pb_int = {
        "trackNumberHits", track_n_hits, &typeid(std::vector<int>)};

      form.read(creator, segment_id, pb_int);
      track_n_hits = static_cast<std::vector<int> const*>(pb_int.data);

      std::vector<TrackStart> const* start_points = nullptr;

      form::experimental::product_with_name pb_points = {
        "trackStartPoints", start_points, &typeid(std::vector<TrackStart>)};

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

      // Verify segment checksums
      auto key = std::make_pair(nevent, nseg);
      if (expected_seg.count(key)) {
        auto const& exp = expected_seg[key];
        bool seg_ok = (std::fabs(check - exp.check) <= TOLERANCE) &&
                      (std::fabs(checkPoints.getX() - exp.cpx) <= TOLERANCE) &&
                      (std::fabs(checkPoints.getY() - exp.cpy) <= TOLERANCE) &&
                      (std::fabs(checkPoints.getZ() - exp.cpz) <= TOLERANCE);
        if (seg_ok) {
          std::cout << "VERIFY PASS: event=" << nevent << " seg=" << nseg << std::endl;
        } else {
          std::cerr << "VERIFY FAIL: event=" << nevent << " seg=" << nseg
                    << " expected check=" << exp.check << " got=" << check
                    << " expected cpx=" << exp.cpx << " got=" << checkPoints.getX()
                    << " expected cpy=" << exp.cpy << " got=" << checkPoints.getY()
                    << " expected cpz=" << exp.cpz << " got=" << checkPoints.getZ() << std::endl;
          all_passed = false;
        }
      } else {
        std::cerr << "VERIFY FAIL: no expected checksum for event=" << nevent << " seg=" << nseg
                  << std::endl;
        all_passed = false;
      }

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

    form.read(creator, event_id, pb);
    track_x = static_cast<std::vector<float> const*>(pb.data); //FIXME: Can this be done by FORM?

    float check = 0.0;
    for (float val : *track_x)
      check += val;
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << std::endl;

    // Verify event checksum
    if (expected_evt.count(nevent)) {
      auto const& exp = expected_evt[nevent];
      bool evt_ok = (std::fabs(check - exp.check) <= TOLERANCE);
      if (evt_ok) {
        std::cout << "VERIFY PASS: event=" << nevent << std::endl;
      } else {
        std::cerr << "VERIFY FAIL: event=" << nevent << " expected check=" << exp.check
                  << " got=" << check << std::endl;
        all_passed = false;
      }
    } else {
      std::cerr << "VERIFY FAIL: no expected checksum for event=" << nevent << std::endl;
      all_passed = false;
    }

    delete track_x; //FIXME: PHLEX owns this memory!

    std::cout << "PHLEX: Read Event done " << nevent << std::endl;
  }

  // Report overall result
  if (all_passed) {
    std::cout << "PHLEX: All verification checks PASSED." << std::endl;
    return 0;
  } else {
    std::cerr << "PHLEX: Some verification checks FAILED." << std::endl;
    return 1;
  }
}
