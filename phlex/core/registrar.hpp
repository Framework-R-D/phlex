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

#include "phlex/core/fwd.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {

  namespace internal {
    PHLEX_CORE_EXPORT void add_to_error_messages(std::vector<std::string>& errors,
                                                 std::string const& entity,
                                                 std::string const& name);
  }

  template <typename Ptr>
  class PHLEX_CORE_EXPORT registrar {
    using nodes = simple_ptr_map<Ptr>;
    using node_creator = std::function<Ptr(std::vector<std::string>, std::vector<std::string>)>;

  public:
    explicit registrar(nodes& node_map, std::vector<std::string>& errors);

    registrar(registrar const&) = delete;
    registrar& operator=(registrar const&) = delete;

    // Moving must clear the source creator so its destructor cannot register the node again.
    registrar(registrar&& other) noexcept;
    registrar& operator=(registrar&& other) noexcept;

    bool has_predicates() const;

    void set_creator(node_creator creator);
    void set_predicates(std::optional<std::vector<std::string>> predicates);

    void set_output_product_suffixes(std::vector<std::string> output_product_suffixes);

    ~registrar() noexcept(false);

  private:
    std::vector<std::string> release_predicates();

    void create_node(std::vector<std::string> output_product_suffixes);

    nodes* nodes_;
    std::vector<std::string>* errors_;
    node_creator creator_;
    std::optional<std::vector<std::string>> predicates_;
    std::vector<std::string> output_product_suffixes_;
  };

  extern template class registrar<std::unique_ptr<declared_fold>>;
  extern template class registrar<std::unique_ptr<declared_observer>>;
  extern template class registrar<std::unique_ptr<declared_output>>;
  extern template class registrar<std::unique_ptr<declared_predicate>>;
  extern template class registrar<std::unique_ptr<declared_transform>>;
  extern template class registrar<std::unique_ptr<declared_unfold>>;
  extern template class registrar<std::unique_ptr<provider_node>>;
}

#endif // PHLEX_CORE_REGISTRAR_HPP
