//Part two of schema evolution unit test: can we build a program against a new dictionary for the same product that reads the old product and gets back the correct values?

#include "test/form/data_products/extra_member/track_start.hpp"
#include "test/form/test_utils.hpp"

#include <iostream>
#include <vector>
#include <fstream>

using namespace form::test;

int main(int const argc, char const** argv)
{
  int const technology = getTechnology(argc, argv);
  if (technology < 0)
    return 1;

  auto const& [prods] = read<std::vector<TrackStart>>(technology);
  std::ofstream outFile("form_root_schema_read_log.txt");
  for (auto const& prod : *prods)
    outFile << prod << std::endl;

  return 0;
}
