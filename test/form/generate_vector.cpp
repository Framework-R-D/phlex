#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <random>
#include <vector>

using namespace phlex;

//A gaussian_generator generates a vector<int> sampled from a Gaussian distribution
class gaussian_generator {
public:
  gaussian_generator(int const n_time_ticks, int const seed, float const mean, float const stddev) :
    n_time_ticks_(n_time_ticks), gen_(seed), dist_(mean, stddev)
  {
  }

  std::vector<int> operator()([[maybe_unused]] data_cell_index const& idx)
  {
    std::vector<int> randoms(n_time_ticks_);
    for (auto& random : randoms) {
      random = static_cast<int>(dist_(gen_));
    }
    return randoms;
  }

private:
  int n_time_ticks_;
  std::mt19937 gen_;
  std::normal_distribution<float> dist_;
};

PHLEX_REGISTER_PROVIDERS(graph, config)
{
  int const seed = config.get<int>(
    "seed", static_cast<int>(std::chrono::system_clock::now().time_since_epoch().count()));
  int const n_time_ticks = config.get<int>("n_time_ticks");
  auto const mean = config.get<float>("mean", 0);
  auto const stddev = config.get<float>("stddev", 1);
  auto creator = config.get<std::string>("creator");

  graph.make<gaussian_generator>(n_time_ticks, seed, mean, stddev)
    .provide("random_wires", &gaussian_generator::operator())
    .output_product(
      creator, phlex::experimental::identifier(creator), phlex::experimental::identifier("event"));
}

PHLEX_REGISTER_ALGORITHMS(graph, config)
{
  auto const lhs_creator = config.get<std::string>("lhs_creator");
  auto const rhs_creator = config.get<std::string>("rhs_creator");

  graph
    .transform(
      "add_wires",
      [](std::vector<int> const& lhs, std::vector<int> const& rhs) {
        assert(lhs.size() == rhs.size());
        std::vector<int> sum(lhs.size());
        for (size_t which_entry = 0; which_entry < lhs.size(); ++which_entry) {
          sum[which_entry] = lhs[which_entry] + rhs[which_entry];
        }
        return sum;
      },
      concurrency::unlimited)
    .input_family(product_selector{.creator = lhs_creator, .layer = "event"},
                  product_selector{.creator = rhs_creator, .layer = "event"})
    .output_product_suffixes("sums");
}
