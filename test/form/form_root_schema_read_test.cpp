//Part two of schema evolution unit test: can we build a program against a new dictionary for the same product that reads the old product and gets back the correct values?

#include "test/form/data_products/extra_member/track_start.hpp"
#include "test/form/test_utils.hpp"

#include <fstream>
#include <iostream>
#include <vector>

using namespace form::test;

int main(int const argc, char const** argv)
{
  std::string const tech_string = (argc > 1) ? argv[1] : "ROOT_TTREE";

  try {
    int const technology = getTechnology(tech_string);
  
    auto const& [prods] = read<std::vector<TrackStart>>(technology);
    std::ofstream outFile("form_root_schema_read_log_" + tech_string + ".txt");
    for (auto const& prod : *prods)
      outFile << prod << '\n';
  } catch (std::exception const& e) {
    std::cerr << "Exception caught in main: " << e.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown exception caught in main.\n";
    return 1;
  }

  return 0;
}
