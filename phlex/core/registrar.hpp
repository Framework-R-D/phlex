#ifndef phlex_core_registrar_hpp
#define phlex_core_registrar_hpp

// =======================================================================================
//
// The registrar class completes the registration of a node at the end of a registration
// statement.  For example:
//
//   g.make<MyTransform>()
//     .with("name", &MyTransform::transform, concurrency{n})
//     .when(...)
//     .transform(...)
//     .to(...);
//             ^ Registration happens at the completion of the full statement.
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
//   g.make<MyOutput>().output_with("all_slow", &MyOutput::output);
//   g.make<MyOutput>().output_with("some_slow", &MyOutput::output).when(...);
//
// Either of the above registration statements are valid, but how the functions are
// registered with the framework depends on the function call-chain.  If the registration
// were to occur during the declare_output call, then it would be difficult to propagate
// the "concurrency" or "when" values.  By using the registrar class, we ensure
// that the user functions are registered at the end of each statement, after all the
// information has been specified.
//
// =======================================================================================

#include "phlex/utilities/simple_ptr_map.hpp"

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace phlex::experimental {

  namespace detail {
    void add_to_error_messages(std::vector<std::string>& errors, std::string const& name);
  }

  template <typename Ptr>
  class registrar {
    using Nodes = simple_ptr_map<Ptr>;
    using Creator = std::function<Ptr(std::vector<std::string>)>;

  public:
    explicit registrar(Nodes& nodes, std::vector<std::string>& errors) :
      nodes_{&nodes}, errors_{&errors}
    {
    }

    registrar(registrar const&) = delete;
    registrar& operator=(registrar const&) = delete;

    registrar(registrar&&) = default;
    registrar& operator=(registrar&&) = default;

    void set_creator(Creator creator) { creator_ = std::move(creator); }
    void set_predicates(std::optional<std::vector<std::string>> predicates)
    {
      predicates_ = std::move(predicates);
    }

    bool has_predicates() const { return predicates_.has_value(); }

    ~registrar() noexcept(false)
    {
      if (creator_) {
        create_node();
      }
    }

  private:
    std::vector<std::string> release_predicates()
    {
      return std::move(predicates_).value_or(std::vector<std::string>{});
    }

    void create_node()
    {
      assert(creator_);
      auto ptr = creator_(release_predicates());
      auto name = ptr->full_name();
      auto [_, inserted] = nodes_->try_emplace(name, std::move(ptr));
      if (not inserted) {
        detail::add_to_error_messages(*errors_, name);
      }
    }

    Nodes* nodes_;
    std::vector<std::string>* errors_;
    Creator creator_{};
    std::optional<std::vector<std::string>> predicates_;
  };
}

#endif // phlex_core_registrar_hpp
