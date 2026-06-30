#ifndef PHLEX_CORE_FRAMEWORK_GRAPH_HPP
#define PHLEX_CORE_FRAMEWORK_GRAPH_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/filter.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/index_router.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/data_cell_tracker.hpp"
#include "phlex/model/data_layer_hierarchy.hpp"
#include "phlex/model/fixed_hierarchy.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"
#include "phlex/utilities/resource_usage.hpp"

#include "oneapi/tbb/flow_graph.h"
#include "oneapi/tbb/info.h"

#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace phlex {
  class configuration;
}

namespace phlex::detail {
  class PHLEX_CORE_EXPORT framework_graph {
  public:
    [[nodiscard]] static framework_graph with_default_driver(
      int max_parallelism = oneapi::tbb::info::default_concurrency());
    [[nodiscard]] static framework_graph without_driver(
      int max_parallelism = oneapi::tbb::info::default_concurrency());

    ~framework_graph();
    framework_graph(framework_graph const&) = delete;
    framework_graph& operator=(framework_graph const&) = delete;
    framework_graph(framework_graph&&) = delete;
    framework_graph& operator=(framework_graph&&) = delete;

    void add_driver(phlex::experimental::driver_bundle bundle);

    template <typename Generator>
      requires requires(std::shared_ptr<Generator> generator, std::vector<source const*> sources) {
        {
          experimental::driver_proxy{sources}.driver(generator)
        } -> std::same_as<phlex::experimental::driver_bundle>;
      }
    void add_driver(std::shared_ptr<Generator> generator)
    {
      add_driver(driver_proxy().driver(std::move(generator)));
    }

    void execute();

    std::size_t seen_cell_count(std::string const& layer_name, bool missing_ok = false) const;
    std::size_t execution_count(std::string const& node_name) const;

    experimental::module_graph_proxy<void_tag> module_proxy(configuration const& config)
    {
      return {config, graph_, nodes_, registration_errors_};
    }

    experimental::source_bundle source_proxy(configuration const& config)
    {
      return {config, graph_, nodes_, registration_errors_};
    }

    experimental::driver_proxy driver_proxy(std::vector<std::string> strings = {})
    {
      return experimental::driver_proxy(nodes_.sources_for(strings));
    }

    // Framework function registrations

    // N.B. declare_output() is not directly accessible through framework_graph.  Is this
    //      right?

    template <typename... InitArgs>
    auto fold(std::string name,
              is_fold_like auto f,
              concurrency c = concurrency::serial,
              std::string partition = "job",
              InitArgs&&... init_args)
    {
      return make_glue().fold(std::move(name),
                              std::move(f),
                              c,
                              std::move(partition),
                              std::forward<InitArgs>(init_args)...);
    }

    template <typename T>
    auto unfold(std::string name,
                is_predicate_like auto pred,
                auto unf,
                concurrency c,
                std::string destination_data_layer)
    {
      return make_glue<T, false>().unfold(
        std::move(name), std::move(pred), std::move(unf), c, std::move(destination_data_layer));
    }

    auto observe(std::string name, is_observer_like auto f, concurrency c = concurrency::serial)
    {
      return make_glue().observe(std::move(name), std::move(f), c);
    }

    auto predicate(std::string name, is_predicate_like auto f, concurrency c = concurrency::serial)
    {
      return make_glue().predicate(std::move(name), std::move(f), c);
    }

    auto transform(std::string name, is_transform_like auto f, concurrency c = concurrency::serial)
    {
      return make_glue().transform(std::move(name), std::move(f), c);
    }

    auto provide(std::string name, auto f, concurrency c = concurrency::serial)
    {
      return make_glue().provide(std::move(name), std::move(f), c);
    }

    template <std::derived_from<source> Source, typename... Args>
    void add_source(std::string name, Args&&... args)
    {
      return make_glue().template add_source<Source>(std::move(name), std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    glue<T> make(Args&&... args)
    {
      return make_glue<T>(std::forward<Args>(args)...);
    }

  private:
    /**
     * Creates a glue object that binds framework components together.
     *
     * @tparam T Type of object to construct and bind, defaults to void_tag
     * @tparam Construct Boolean flag controlling whether to construct T, defaults to true
     * @tparam Args Variadic template for constructor arguments
     *
     * @param args Arguments forwarded to T's constructor if Construct is true
     *
     * @return glue<T> A glue object containing the constructed T (if any) and framework components
     *
     * This helper creates a glue object that connects framework components. If T is void_tag,
     * no object is constructed and the glue acts as a pure connector between framework parts.
     * This is useful for simple operations that don't require maintaining state or complex logic.
     *
     * If T is not void_tag and Construct is true, it will construct a shared_ptr to T using
     * the provided args. This allows the glue to maintain state and complex behavior through
     * the lifetime of the constructed T object.
     *
     * The glue object binds together:
     * - The TBB flow graph
     * - Node catalog for tracking registered nodes
     * - Optional instance of T (if not void_tag)
     * - Registration error collection
     */
    template <typename T = phlex::detail::void_tag, bool Construct = true, typename... Args>
    glue<T> make_glue(Args&&... args)
    {
      std::shared_ptr<T> bound_object{nullptr};
      if constexpr (is_bound_object<T> && Construct) {
        bound_object = std::make_shared<T>(std::forward<Args>(args)...);
      }
      return {graph_, nodes_, std::move(bound_object), registration_errors_};
    }

    void run();
    void finalize();
    void throw_if_registration_errors() const;
    void make_filter_edges();
    void make_bookkeeping_edges();
    void finalize_router(index_router::provider_input_ports_t provider_input_ports,
                         std::map<std::string, named_index_ports> multilayer_join_index_ports);

    enum class driver_mode { default_driver, deferred_driver };
    explicit framework_graph(driver_mode mode, int max_parallelism);

    phlex::detail::resource_usage graph_resource_usage_{};
    phlex::detail::max_allowed_parallelism parallelism_limit_;
    fixed_hierarchy fixed_hierarchy_;
    phlex::detail::data_layer_hierarchy hierarchy_{};
    node_catalog nodes_{};
    std::map<std::string, filter> filters_{};
    // The graph_ object uses the filters_, nodes_, and hierarchy_ objects implicitly.
    tbb::flow::graph graph_{};
    std::optional<phlex::detail::framework_driver> driver_{};
    std::vector<std::string> registration_errors_{};
    phlex::detail::data_cell_tracker cell_tracker_{};
    tbb::flow::input_node<phlex::detail::ready_flushes_then_emit> src_;
    index_router index_router_;
    tbb::flow::function_node<phlex::detail::ready_flushes_then_emit,
                             phlex::data_cell_index_ptr,
                             tbb::flow::lightweight>
      index_receiver_;
    tbb::flow::function_node<data_cell_index_ptr, tbb::flow::continue_msg, tbb::flow::lightweight>
      hierarchy_node_;
    driver_mode driver_mode_{driver_mode::default_driver};
    bool shutdown_on_error_{false};
  };
}

#endif // PHLEX_CORE_FRAMEWORK_GRAPH_HPP
