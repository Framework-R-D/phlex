#include "phlex/app/run.hpp"
#include "phlex/app/version.hpp"

#include "libjsonnet++.h"
#include "oneapi/tbb/info.h"

#include <bits/basic_string.h>
#include <boost/json/parse.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

using namespace std::string_literals;
using namespace boost;
namespace bpo = boost::program_options;

auto main(int argc, char* argv[]) -> int
{
  std::ostringstream descstr;
  descstr << "\nUsage: " << std::filesystem::path(argv[0]).filename().native()
          << " -c <config-file> [other-options]\n\n"
          << "Basic options";
  bpo::options_description desc{descstr.str()};

  auto max_concurrency = oneapi::tbb::info::default_concurrency();
  std::string config_file;
  // clang-format off
  desc.add_options()
    ("help,h", "Produce help message")
    ("config,c", bpo::value<std::string>(&config_file), "Configuration file")
    ("parallel,j",
       bpo::value<int>()->default_value(max_concurrency),
       "Maximum parallelism requested for the program")
    ("version", ("Print phlex version ("s + phlex::experimental::version() + ")").c_str());
  // clang-format on

  // Parse the command line.
  bpo::variables_map vm;
  try {
    bpo::store(
      bpo::command_line_parser(argc, argv)
        .options(desc)
        .style(bpo::command_line_style::default_style & ~bpo::command_line_style::allow_guessing)
        .run(),
      vm);
    bpo::notify(vm);
  } catch (bpo::error const& e) {
    std::cerr << "Exception from command line processing in " << argv[0] << ": " << e.what()
              << '\n';
    return 1;
  }

  if (vm.count("help")) {
    std::cout << desc << '\n';
    return 0;
  }

  if (vm.count("version")) {
    std::cout << "phlex " << phlex::experimental::version() << '\n';
    return 0;
  }

  if (not vm.count("config")) {
    std::cerr << "Error: No configuration file given.\n";
    return 2;
  }

  jsonnet::Jsonnet j;
  if (not j.init()) {
    std::cerr << "Error: Could not initialize Jsonnet parser.\n";
    return 2;
  }

  std::cout << "Using configuration file: " << config_file << '\n';

  std::string config_str;
  auto rc = j.evaluateFile(config_file, &config_str);
  if (not rc) {
    std::cerr << j.lastError() << '\n';
    return 2;
  }

  // Check configuration...
  auto configurations = json::parse(config_str).as_object();
  if (auto const* specified_concurrency = configurations.if_contains("max_concurrency")) {
    max_concurrency = specified_concurrency->to_number<int>();
    configurations.erase("max_concurrency"); // Remove consumed parameters
  }

  // ...but command-line always wins.
  if (not vm["parallel"].defaulted()) {
    max_concurrency = vm["parallel"].as<int>();
  }
  phlex::experimental::run(configurations, max_concurrency);
}
