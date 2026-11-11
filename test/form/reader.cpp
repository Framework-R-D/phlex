// Copyright (C) 2025 ...

#include "core/technology.hpp"
#include "data_products/track_start.hpp"
#include "form/config.hpp"
#include "form/form_reader.hpp"
#include "form/product_with_name.hpp"
#include "test_helpers.hpp"
#include "test_utils.hpp"

#include <cmath>
#include <format>
#include <fstream>
#include <iostream>
#include <istream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static int const number_event = 4;
static int const number_segment = 15;

static float const tolerance = 1e-3f;

namespace {
  // Structs to hold expected checksums
  struct seg_checksum {
    float check;
    float cpx, cpy, cpz;
  };

  struct evt_checksum {
    float check;
  };

  struct expected_checksums {
    std::map<std::pair<int, int>, seg_checksum> segments;
    std::map<int, evt_checksum> events;
  };

  expected_checksums load_expected_checksums(std::istream& input)
  {
    expected_checksums expected;
    std::string line;
    while (std::getline(input, line)) {
      std::istringstream iss(line);
      std::string type;
      iss >> type;
      if (type == "SEG") {
        seg_checksum cs{};
        int nevent{};
        int nseg{};
        iss >> nevent >> nseg >> cs.check >> cs.cpx >> cs.cpy >> cs.cpz;
        expected.segments[{nevent, nseg}] = cs;
      } else if (type == "EVT") {
        evt_checksum cs{};
        int nevent{};
        iss >> nevent >> cs.check;
        expected.events[nevent] = cs;
      }
    }
    return expected;
  }

  template <typename T>
  std::unique_ptr<T const> read_product(form::experimental::form_reader_interface& reader,
                                        std::string const& creator,
                                        std::string const& index,
                                        std::string const& label)
  {
    form::experimental::product_with_name product{
      .label = label, .data = nullptr, .type = &typeid(T)};
    reader.read(creator, form::test::test_stage, index, product);
    return std::unique_ptr<T const>{static_cast<T const*>(product.data)};
  }

  bool verify_segment(form::experimental::form_reader_interface& reader,
                      expected_checksums const& expected,
                      int nevent,
                      int nseg)
  {
    // Must match the canonical format emitted by writer.cpp (phlex data_cell_index format)
    std::string const seg_id_text = std::format("[event:{}, segment:{}]", nevent, nseg);
    std::string const creator = "Toy_Tracker";
    auto const track_start_x =
      read_product<std::vector<float>>(reader, creator, seg_id_text, "trackStart");
    auto const track_n_hits =
      read_product<std::vector<int>>(reader, creator, seg_id_text, "trackNumberHits");
    auto const start_points =
      read_product<std::vector<track_start>>(reader, creator, seg_id_text, "trackStartPoints");

    float check = 0.0;
    for (float const val : *track_start_x) {
      check += val;
    }
    for (int const val : *track_n_hits) {
      check += static_cast<float>(val);
    }
    track_start check_points;
    for (track_start const val : *start_points) {
      check_points += val;
    }
    std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
              << ", check = " << check << '\n';
    std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
              << ", check_points = " << check_points << '\n';

    auto const it = expected.segments.find({nevent, nseg});
    if (it == expected.segments.end()) {
      std::cerr << "VERIFY FAIL: no expected checksum for event=" << nevent << " seg=" << nseg
                << '\n';
      return false;
    }

    auto const& exp = it->second;
    bool const seg_ok = (std::fabs(check - exp.check) <= tolerance) &&
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
    }
    return seg_ok;
  }

  bool verify_event(form::experimental::form_reader_interface& reader,
                    expected_checksums const& expected,
                    int nevent)
  {
    std::string const evt_id_text = std::format("[event:{}]", nevent);
    auto const track_x =
      read_product<std::vector<float>>(reader, "Toy_Tracker_Event", evt_id_text, "trackStartX");

    float check = 0.0;
    for (float const val : *track_x) {
      check += val;
    }
    std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
              << ", check = " << check << '\n';

    auto const it = expected.events.find(nevent);
    if (it == expected.events.end()) {
      std::cerr << "VERIFY FAIL: no expected checksum for event=" << nevent << '\n';
      return false;
    }

    auto const& exp = it->second;
    bool const evt_ok = (std::fabs(check - exp.check) <= tolerance);
    if (evt_ok) {
      std::cout << "VERIFY PASS: event=" << nevent << '\n';
    } else {
      std::cerr << "VERIFY FAIL: event=" << nevent << " expected check=" << exp.check
                << " got=" << check << '\n';
    }
    return evt_ok;
  }
} // namespace

int main(int argc, char** argv)
{
  std::cout << "In main" << '\n';

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";
  std::string const checksum_filename = (argc > 2) ? argv[2] : "toy_checksums.txt";
  auto const technology = form::test::get_technology((argc > 3) ? argv[3] : "ROOT_TTREE");

  std::ifstream checksum_file(checksum_filename);
  if (!checksum_file.is_open()) {
    std::cerr << "ERROR: Could not open checksum file: " << checksum_filename << '\n';
    return 1;
  }

  auto const expected = load_expected_checksums(checksum_file);
  checksum_file.close();

  // TODO: Read configuration from config file instead of hardcoding
  form::experimental::config::item_config config_items;
  config_items.add_item("trackStart", filename, technology);
  config_items.add_item("trackNumberHits", filename, technology);
  config_items.add_item("trackStartPoints", filename, technology);
  config_items.add_item("trackStartX", filename, technology);

  form::experimental::config::tech_setting_config const tech_config;

  form::experimental::form_reader_interface reader(config_items, tech_config);

  bool all_passed = true;

  for (int nevent = 0; nevent < number_event; ++nevent) {
    std::cout << "PHLEX: Read Event No. " << nevent << '\n';

    for (int nseg = 0; nseg < number_segment; ++nseg) {
      if (!verify_segment(reader, expected, nevent, nseg)) {
        all_passed = false;
      }
    }
    std::cout << "PHLEX: Read Event segments done " << nevent << '\n';

    if (!verify_event(reader, expected, nevent)) {
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
