// Writes each data cell's own number so that read-back can verify which cell a row belongs to.
//
// The "stamp" transform sleeps for a random time so that stores reach the output out of cell
// order; the "check_cell_numbers" observer fails the job if a served value belongs to another cell.

#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <chrono>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace phlex;

namespace {
  std::vector<int> number_of(data_cell_index const& id)
  {
    return std::vector<int>{static_cast<int>(id.number())};
  }
}

PHLEX_REGISTER_PROVIDERS(graph, config)
{
  auto const creator = config.get<std::string>("creator");
  graph.provide("cell_numbers", number_of, concurrency::unlimited)
    .output_product(
      creator, experimental::identifier("numbers"), experimental::identifier("event"));
}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  auto const input_creator = config.get<std::string>("input_creator");
  if (config.get<bool>("check", false)) {
    m.observe(
       "check_cell_numbers",
       [](handle<std::vector<int>> const values) {
         if (*values != number_of(values.data_cell_index())) {
           auto const served =
             values->empty() ? std::string{"nothing"} : std::to_string(values->front());
           throw std::runtime_error("cell " + values.data_cell_index().to_string() +
                                    " was served the value " + served);
         }
       },
       concurrency::unlimited)
      .input_family(product_selector{.creator = experimental::identifier{input_creator},
                                     .layer = "event",
                                     .suffix = "cell_numbers"});
    return;
  }

  m.transform(
     "stamp",
     [](std::vector<int> const& values) {
       static thread_local std::mt19937 gen{std::random_device{}()};
       std::this_thread::sleep_for(
         std::chrono::microseconds(std::uniform_int_distribution<int>(0, 2000)(gen)));
       return values;
     },
     concurrency::unlimited)
    .input_family(product_selector{
      .creator = experimental::identifier{input_creator}, .layer = "event", .suffix = "numbers"})
    .output_product_suffixes("cell_numbers");
}
