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
// This is achieved by creating a class registrar class object (internally during any of
// the declare* calls), which is then passed along through each successive function call
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

#include <functional>
#include <string>
#include <vector>

namespace phlex::experimental {

  template <typename T>
  concept map_like = requires { typename T::mapped_type; };

  template <typename Ptr>
  class registrar {
    using Nodes = std::map<std::string, Ptr>;
    using Creator = std::function<Ptr()>;

  public:
    explicit registrar(Nodes& nodes, std::vector<std::string>& errors) :
      nodes_{nodes}, errors_{errors}
    {
    }

    registrar(registrar const&) = delete;
    registrar& operator=(registrar const&) = delete;

    registrar(registrar&&) = default;
    registrar& operator=(registrar&&) = default;

    void set(Creator creator) { creator_ = std::move(creator); }
    ~registrar() noexcept(false)
    {
      if (creator_) {
        auto ptr = creator_();
        auto name = ptr->full_name();
        auto [_, inserted] = nodes_.try_emplace(name, std::move(ptr));
        if (not inserted) {
          errors_.push_back(fmt::format("Node with name '{}' already exists", name));
        }
      }
    }

  private:
    Nodes& nodes_;
    std::vector<std::string>& errors_;
    Creator creator_{};
  };

  template <map_like Nodes>
  registrar(Nodes&, std::vector<std::string>&) -> registrar<typename Nodes::mapped_type>;

}

#endif // phlex_core_registrar_hpp
