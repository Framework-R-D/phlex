#include "phlex/app/run.hpp"
#include "phlex/app/version.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
// Public umbrella whose implementation headers are ignored by include-cleaner.
#include <boost/program_options.hpp> // IWYU pragma: keep
#include <fmt/format.h>
#include <libjsonnet++.h>
#include <oneapi/tbb/info.h>

#include <exception>
#include <expected>
#include <filesystem>
#include <iostream>
#include <print>
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

  std::expected<bpo::variables_map, std::string> parse_command_line(
    int argc, char** argv, bpo::options_description const& desc)
  {
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
      return std::unexpected{std::string{e.what()}};
    }
    return vm;
  }

  phlex::detail::overridable_configuration extract_overrides(json::object& configurations,
                                                             bpo::variables_map const& vm)
  {
    phlex::detail::overridable_configuration overrides{.max_parallelism = vm["parallel"].as<int>()};
    if (auto const* specified_concurrency = configurations.if_contains("max_concurrency")) {
      overrides.max_parallelism = specified_concurrency->to_number<int>();
      configurations.erase("max_concurrency"); // Remove consumed parameters
    }
    if (auto const* specified_stage = configurations.if_contains("stage")) {
      overrides.stage = specified_stage->as_string().c_str();
      configurations.erase("stage"); // Remove consumed parameters
    }

    // Command-line always wins.
    if (not vm["parallel"].defaulted()) {
      overrides.max_parallelism = vm["parallel"].as<int>();
    }
    if (vm.contains("stage")) {
      overrides.stage = vm["stage"].as<std::string>();
    }
    return overrides;
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape) -- primary application entry point; exceptions
// from potentially-throwing calls should be handled internally, not propagated from main
int main(int argc, char* argv[])
{
  std::string config_file;
  auto desc = make_options_description(argv[0], config_file);

  // Parse the command line.
  auto parsed = parse_command_line(argc, argv, desc);
  if (not parsed) {
    std::println(
      std::cerr, "Exception from command line processing in {}: {}", argv[0], parsed.error());
    return 1;
  }
  auto const& vm = *parsed;

  if (vm.contains("help")) {
    std::cout << desc << '\n';
    return 0;
  }

  if (vm.contains("version")) {
    std::println("phlex {}", phlex::detail::version());
    return 0;
  }

  if (not vm.contains("config")) {
    std::println(std::cerr, "Error: No configuration file given.");
    return 2;
  }

  jsonnet::Jsonnet j;
  if (not j.init()) {
    std::println(std::cerr, "Error: Could not initialize Jsonnet parser.");
    return 2;
  }

  std::println("Using configuration file: {}", config_file);

  std::string config_str;
  auto rc = j.evaluateFile(config_file, &config_str);
  if (not rc) {
    std::println(std::cerr, "{}", j.lastError());
    return 2;
  }

  auto configurations = json::parse(config_str).as_object();
  auto const overrides = extract_overrides(configurations, vm);
  try {
    phlex::detail::run(configurations, overrides);
  } catch (std::exception const& e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
  } catch (...) {
    std::println(std::cerr, "Unknown exception caught.");
    return 1;
  }
}
