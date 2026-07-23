//Schema evolution test component: write old version of a data product to disk

#include "test/form/data_products/track_start.hpp"
#include "test/form/test_utils.hpp"
#include "test/form/toy_tracker.hpp"

#include <fstream>
#include <iostream>
#include <vector>

using namespace form::test;

int main(int const argc, char const** argv)
{
  std::string const tech_string = (argc > 1) ? argv[1] : "ROOT_TTREE";
  int const technology = getTechnology(tech_string);

  ToyTracker tracker(4 * 1024);
  std::vector<TrackStart> const prods = tracker();

  std::ofstream outFile("form_root_schema_write_log_" + tech_string + ".txt");
  for (auto const& prod : prods) {
    outFile << prod << '\n';
  }

  write(technology, prods);

  return 0;
}
