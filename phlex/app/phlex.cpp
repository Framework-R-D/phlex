#include "phlex/app/run.hpp"
#include "phlex/app/version.hpp"
#include "phlex/concurrency.hpp"

#include "boost/program_options.hpp"
#include "libjsonnet++.h"
#include "oneapi/tbb/info.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std::string_literals;
using namespace boost;
namespace bpo = boost::program_options;

namespace {
  bpo::options_description make_options_description(char const* executable,
                                                    int const max_concurrency,
                                                    std::string& config_file)
  {
    std::ostringstream descstr;
    descstr << "\nUsage: " << std::filesystem::path(executable).filename().native()
            << " -c <config-file> [other-options]\n\n"
            << "Basic options";
    bpo::options_description result{descstr.str()};

    // clang-format off
    result.add_options()
      ("help,h", "Produce help message")
      ("config,c", bpo::value<std::string>(&config_file), "Configuration file")
      ("parallel,j",
       bpo::value<int>()->default_value(max_concurrency),
       "Maximum parallelism requested for the program")
      ("version", ("Print phlex version ("s + phlex::detail::version() + ")").c_str());
    // clang-format on

    return result;
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape) -- primary application entry point; exceptions
// from potentially-throwing calls should be handled internally, not propagated from main
int main(int argc, char* argv[])
{
  auto max_concurrency = oneapi::tbb::info::default_concurrency();
  std::string config_file;
  auto desc = make_options_description(argv[0], max_concurrency, config_file);

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

  if (vm.contains("help")) {
    std::cout << desc << '\n';
    return 0;
  }

  if (vm.contains("version")) {
    std::cout << "phlex " << phlex::detail::version() << '\n';
    return 0;
  }

  if (not vm.contains("config")) {
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
  try {
    phlex::detail::run(configurations, max_concurrency);
  } catch (std::exception const& e) {
    std::cerr << e.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown exception caught.\n";
    return 1;
  }
}
