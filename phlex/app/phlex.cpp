#include "phlex/app/run.hpp"
#include "phlex/app/version.hpp"

#include <boost/json/parse.hpp>
#include <boost/program_options.hpp> // IWYU pragma: keep
#include <fmt/format.h>
#include <libjsonnet++.h>
#include <oneapi/tbb/info.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

using namespace std::string_literals;
using namespace boost;
namespace bpo = boost::program_options;

namespace {
  bpo::options_description make_options_description(std::string_view const executable,
                                                    std::string& config_file)
  {
    bpo::options_description result{
      fmt::format("\nUsage: {} -c <config-file> [other-options]\n\n"
                  "Basic options",
                  std::filesystem::path{executable}.filename().native())};

    // clang-format off
    result.add_options()
      ("help,h", "Produce help message")
      ("config,c", bpo::value<std::string>(&config_file), "Configuration file")
      ("parallel,j",
       bpo::value<int>()->default_value(oneapi::tbb::info::default_concurrency()),
       "Maximum parallelism requested for the program")
      ("stage", bpo::value<std::string>(), "Name to assign to the phlex invocation")
      ("version", ("Print phlex version ("s + phlex::detail::version() + ")").c_str());
    // clang-format on

    return result;
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape) -- primary application entry point; exceptions
// from potentially-throwing calls should be handled internally, not propagated from main
int main(int argc, char* argv[])
{
  std::string config_file;
  auto desc = make_options_description(argv[0], config_file);

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
  phlex::detail::overridable_configuration overrides{.max_parallelism = vm["parallel"].as<int>()};
  auto configurations = json::parse(config_str).as_object();
  if (auto const* specified_concurrency = configurations.if_contains("max_concurrency")) {
    overrides.max_parallelism = specified_concurrency->to_number<int>();
    configurations.erase("max_concurrency"); // Remove consumed parameters
  }
  if (auto const* specified_stage = configurations.if_contains("stage")) {
    overrides.stage = specified_stage->as_string().c_str();
    configurations.erase("stage"); // Remove consumed parameters
  }

  // ...but command-line always wins.
  if (not vm["parallel"].defaulted()) {
    overrides.max_parallelism = vm["parallel"].as<int>();
  }
  if (vm.contains("stage")) {
    overrides.stage = vm["stage"].as<std::string>();
  }
  try {
    phlex::detail::run(configurations, overrides);
  } catch (std::exception const& e) {
    std::cerr << e.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown exception caught.\n";
    return 1;
  }
}
