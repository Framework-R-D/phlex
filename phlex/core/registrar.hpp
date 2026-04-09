#ifndef PHLEX_CORE_REGISTRAR_HPP
#define PHLEX_CORE_REGISTRAR_HPP

#include "phlex/phlex_core_export.hpp"

// =======================================================================================
//
// The registrar class completes the registration of a node at the end of a registration
// statement.  For example:
//
//   g.make<MyTransform>()
//     .transform("name", &MyTransform::transform, concurrency{n})
//     .input_family(...)
//     .when(...)
//     .output_product_suffixes(...);
//                                  ^ Registration occurs at the completion of the full statement.
//
// This is achieved by creating a registrar class object (internally during any of the
// declare* calls), which is then passed along through each successive function call
// (concurrency, when, etc.).  When the statement completes (i.e. the semicolon is
// reached), the registrar object is destroyed, where the registrar's destructor registers
// the declared function as a graph node to be used by the framework.
//
// Timing
// ======
//
//    "Hurry.  Careful timing we will need."  -Yoda (Star Wars, Episode III)
//
// In order for this system to work correctly, any intermediate objects created during the
// function-call chain above should contain the registrar object as its *last* data
// member.  This is to ensure that the registration happens before the rest of the
// intermediate object is destroyed, which could invalidate some of the data required
// during the registration process.
//
// Design rationale
// ================
//
// Consider the case of two output nodes:
//
//   g.make<MyOutput>().output("all_slow", &MyOutput::output);
//   g.make<MyOutput>().output("some_slow", &MyOutput::output).when(...);
//
// Either of the above registration statements are valid, but how the functions are
// registered with the framework depends on the function call-chain.  If the registration
// were to occur during the declare_output call, then it would be difficult to propagate
// the "concurrency" or "when" values.  By using the registrar class, we ensure
// that the user functions are registered at the end of each statement, after all the
// information has been specified.
//
// =======================================================================================

#include "phlex/core/product_registry.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <cassert>
#include <functional>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace phlex::experimental {

  namespace detail {
    PHLEX_CORE_EXPORT void add_to_error_messages(std::vector<std::string>& errors,
                                                 std::string const& name);
  }

  template <typename Ptr>
  class registrar {
    using nodes = simple_ptr_map<Ptr>;
    using node_creator = std::function<Ptr(
      product_registry const&, std::vector<std::string>, std::vector<std::string>)>;

  public:
    explicit registrar(nodes& node_map,
                       std::vector<std::string>& errors,
                       product_registry& registry) :
      nodes_{&node_map}, errors_{&errors}, registry_{&registry}
    {
    }

    registrar(registrar const&) = delete;

    registrar(registrar&&) = default;
    registrar& operator=(registrar const&) = delete;
    registrar& operator=(registrar&&) = default;

    bool has_predicates() const { return predicates_.has_value(); }

    void set_creator(node_creator creator) { creator_ = std::move(creator); }
    void set_predicates(std::optional<std::vector<std::string>> predicates)
    {
      predicates_ = std::move(predicates);
    }

    void set_output_product_suffixes(std::vector<std::string> output_product_suffixes)
    {
      create_node(std::move(output_product_suffixes));
      creator_ = nullptr;
    }

    ~registrar() noexcept(false)
    {
      if (creator_) {
        create_node(std::move(output_product_suffixes_));
      }
    }

  private:
    std::vector<std::string> release_predicates()
    {
      return std::move(predicates_).value_or(std::vector<std::string>{});
    }

    void create_node(std::vector<std::string> output_product_suffixes)
    {
      using namespace phlex::experimental::literals;
      assert(creator_);
      auto ptr = creator_(*registry_, release_predicates(), std::move(output_product_suffixes));
      auto name = ptr->full_name();
      spdlog::debug("Creating node for {}", name);
      //
      // Register output products
      // Providers have a different interface
      if constexpr (requires {
                      { ptr->output_product() } -> std::same_as<product_query const&>;
                    }) {
        auto const& prod = ptr->output_product();
        // The product had better have it's creator and layer defined
        assert(prod.creator && prod.layer);
        registry_->add_product(
          product_specification(algorithm_name::create(std::string_view(*prod.creator)),
                                prod.suffix.value_or(""_id),
                                prod.type),
          ptr->layer(),
          prod.stage.value_or(""_id));
      }
      // Now deal with everything else that produces outputs
      else if constexpr (requires {
                           { ptr->output() } -> std::same_as<product_specification const&>;
                         }) {
        auto spec = ptr->output();
        // FIXME: Once we implement stage names this should be replaced with the current stage name
        identifier stage = "current"_id;
        identifier layer{};
        if constexpr (requires { ptr->layer(); }) {
          layer = ptr->layer();
        } else {
          std::set<identifier> layer_set{ptr->layers()};
          if (layer_set.size() != 1) {
            throw std::runtime_error(
              "Transforms with inputs from more than one layer are unsupported!!");
          }
          layer = *layer_set.begin();
        }

        registry_->add_product(spec, layer, stage);
      }
      auto [_, inserted] = nodes_->try_emplace(name, std::move(ptr));
      if (not inserted) {
        detail::add_to_error_messages(*errors_, name);
      }
    }

    nodes* nodes_;
    std::vector<std::string>* errors_;
    node_creator creator_{};
    std::optional<std::vector<std::string>> predicates_;
    std::vector<std::string> output_product_suffixes_{};
    product_registry* registry_;
  };
}

#endif // PHLEX_CORE_REGISTRAR_HPP
