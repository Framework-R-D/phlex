# Phlex Registration Inversion Plan

## Objective

Remove `graph_proxy` and replace its inheritance hierarchy with independent, capability-specific
module, provider, source, and resource proxies. `framework_graph` remains the only unrestricted
direct-registration interface.

All commands currently available directly on `framework_graph`, including resource registration,
must remain available with the same signatures, defaults, return types, and behavior after the
refactoring.

`framework_graph` and the targeted proxies should share a non-user-facing registration context and
common glue-construction mechanics. This preserves one registration implementation without creating
a second unrestricted registration facade.

## Current State

`graph_proxy<T>` currently has three responsibilities:

1. It stores the registration context, including the TBB graph, stage, node and resource catalogs,
   registration errors, configuration, and optional bound object.
2. It exposes all node-registration operations.
3. It implements object binding through `make<T>()` and `bind_to()`.

The targeted proxies privately inherit from `graph_proxy<T>` and selectively expose operations:

- `module_graph_proxy<T>` exposes module operations.
- `providers_graph_proxy<T>` exposes provider registration.
- `source_graph_proxy<T>` exposes source registration; the target design removes its unused template
  parameter.
- `resources_graph_proxy` is already an independent facade over `resource_catalog`.

At the same time, `framework_graph` already provides the unrestricted direct-registration interface
through its public commands and private `make_glue()` helper. No current caller needs a separate
unrestricted proxy.

Relevant implementation locations include:

- `phlex/core/graph_proxy.hpp`
- `phlex/core/framework_graph.hpp`
- `phlex/module.hpp`
- `phlex/source.hpp`
- `phlex/resource.hpp`
- `phlex/detail/plugin_macros.hpp`
- `phlex/app/load_module.cpp`

## Target Architecture

The intended dependency structure is:

```text
framework_graph
    |
    +-- registration_context
            |
            +-- module_graph_proxy<T>      module operations only
            +-- providers_graph_proxy<T>   provider operations only
            +-- source_graph_proxy         source registration only
            +-- resources_graph_proxy      resource registration only
```

`registration_context` must not itself be a registration facade. It should hold or reference the
framework-owned state required to construct `glue<T>`, but it should not publicly expose operations
such as `transform`, `provide`, or `add_source`.

The targeted proxies must not inherit from or contain a replacement unrestricted facade. They should
compose only the internal state required to implement their own capabilities.

## Proposed Internal Types

Introduce an internal context conceptually similar to:

```cpp
class registration_context {
  configuration const* config_;
  tbb::flow::graph* graph_;
  phlex::experimental::identifier stage_;
  node_catalog* nodes_;
  resource_catalog* resources_;
  std::vector<std::string>* errors_;
};
```

The final representation may use references, pointers, or `std::reference_wrapper`, but it should
satisfy these requirements:

- It is non-owning except for a stage value if copying the stage remains desirable.
- It does not expose mutable catalogs or the TBB graph to plugin code.
- It has an explicitly documented lifetime tied to synchronous registration.
- Its construction is controlled by a dedicated internal proxy factory.
- It provides narrowly scoped internal access needed to create `glue<T>`.

Add a templated state or helper for the optional bound object:

```cpp
template <typename T>
class registration_state {
  registration_context context_;
  std::shared_ptr<T> bound_object_;
};
```

This type should provide internal operations such as:

- Creating `glue<T>` with or without the bound object.
- Binding a newly constructed object and producing `registration_state<U>`.
- Creating the special `glue<Unfolder>` required by `unfold()`.

These operations should be private to the proxy implementation or otherwise inaccessible to plugin
authors.

## Desired Proxy Interfaces

### Direct `framework_graph` Interface

Preserving the current direct-registration API is a requirement, not a temporary compatibility
measure. The following command families must remain directly available on `framework_graph`:

- `fold()`
- `unfold()`
- `observe()`
- `predicate()`
- `transform()`
- `provide()`
- `add_source()`
- `add_unlimited_resource()`
- `add_serialized_resource()`
- `make<T>()`

Their existing signatures, parameter ordering, default arguments, return types, and semantics must be
preserved. In particular, `framework_graph::make<T>()` must continue to return `glue<T>`. Resource
commands should continue to operate directly on the framework-owned resource catalog or through a
private resource helper.

### `module_graph_proxy<T>`

This independent facade should expose only:

- `make<T>()`
- `fold()`
- `observe()`
- `output()`
- `predicate()`
- `transform()`
- `unfold()`

Calling `make<U>()` must return `module_graph_proxy<U>`.

### `providers_graph_proxy<T>`

This independent facade should expose only:

- `make<T>()`
- `provide()`

Calling `make<U>()` must return `providers_graph_proxy<U>`.

### `source_graph_proxy`

This facade should expose only `add_source()` and should not be templated. It has no `make<T>()` or
bound-object behavior. Adapt the plugin macro machinery as needed rather than retaining an unused type
parameter solely to satisfy the current template-template macro interface.

### `resources_graph_proxy`

This facade should continue to expose only:

- `add_unlimited_resource()`
- `add_serialized_resource()`

It should consume the new controlled context or a direct resource-catalog capability instead of the
public `graph_registration_bundle` aggregate.

## Implementation Phases

### Phase 1: Lock Down Existing Capabilities (Complete)

Status: Complete. `test/proxy_capabilities_test.cpp` locks down each proxy's positive and negative
capabilities, bound-proxy behavior, category-preserving `make<T>()`, and direct `framework_graph`
contracts.

Add compile-time tests before changing the hierarchy. Use concepts and `static_assert` checks similar
to those in `test/plugins/register_resources_for_testing.cpp`.

Verify that:

- Module proxies expose all intended module operations.
- Module proxies do not expose `provide`, `add_source`, or resource registration.
- Provider proxies expose only `provide` and, when unbound, `make`.
- Provider proxies do not expose module, source, or resource operations.
- Source proxies expose only `add_source`.
- Resource proxies expose only resource operations.
- Bound module and provider proxies cannot call `make()` again.
- `make<T>()` preserves the proxy category.

Retain or expand runtime tests for bound member-function registration, especially the provider path
currently exercised by `test/plugins/ij_source.cpp`.

### Phase 2: Introduce Shared Registration State (Complete)

Status: Complete. `registration_context` and `registration_state<T>` centralize non-owning framework
state, bound-object ownership, and `glue<T>` construction.

Create the internal `registration_context` and bound-object state.

Move the common constructor arguments and glue creation out of `graph_proxy<T>` before removing it.
Replace repeated forwarding of the graph, stage, catalogs, errors, configuration, and bound object
with the new state shared by `framework_graph` and the targeted proxies.

Initially keep behavior identical, including:

- Configuration-aware name verification for plugin registration.
- Null configuration and existing naming behavior for direct registrations.
- Stage propagation.
- Suppression of the bound object for `add_source()` and `unfold()`.
- Shared ownership of bound objects used by registered callables.

### Phase 3: Convert Targeted Proxies to Standalone Facades (Complete)

Status: Complete. Module, provider, and source proxies are independent facades; source registration
uses the non-template `source_graph_proxy`.

Remove private inheritance from:

- `module_graph_proxy<T>`
- `providers_graph_proxy<T>`
- `source_graph_proxy<T>`

Replace `source_graph_proxy<T>` with the non-template `source_graph_proxy`. Give each facade its own
`registration_state<T>` or relevant narrower state. Implement only the methods in its capability set,
delegating the mechanics to common glue creation.

Keep the per-facade `make<U>()` methods explicit so their return types preserve capability:

```text
module_graph_proxy<void_tag>::make<U>()
  -> module_graph_proxy<U>

providers_graph_proxy<void_tag>::make<U>()
  -> providers_graph_proxy<U>
```

Avoid a public generic factory that permits plugin code to choose an arbitrary proxy type.

### Phase 4: Remove `graph_proxy` (Complete)

Status: Complete. `graph_proxy.hpp` and all references to `graph_proxy<T>` have been removed.

After all targeted proxies use the shared state directly, remove `graph_proxy<T>` and its unrestricted
registration API. Move any remaining reusable mechanics into the non-user-facing state/helper rather
than another base class or facade.

Update includes and forward declarations so plugin-facing headers depend on the shared internal state
instead of `phlex/core/graph_proxy.hpp`. Delete `graph_proxy.hpp` if it no longer contains any required
types; otherwise rename the remaining context implementation so its purpose is clear.

### Phase 5: Preserve and Delegate the `framework_graph` API (Complete)

Status: Complete. Direct registration signatures and behavior remain unchanged, with `make_glue()`
delegating common construction to `registration_state<T>`.

Preserve every existing direct-registration command on `framework_graph`. Implement node and source
operations through its existing `make_glue()` entry point, revised to construct glue from the shared
registration state.

Handle the exceptions explicitly:

- Keep `add_unlimited_resource()` and `add_serialized_resource()` on `framework_graph`, operating on
  `resources_` directly or through a private resource-only helper.
- Preserve `framework_graph::make<T>()` as a `glue<T>`-returning operation.
- Preserve the current `framework_graph::unfold()` parameter ordering.
- Do not add `output()` to `framework_graph` as part of this structural refactor; it is not currently
  a direct command.

Retain a narrowly scoped `make_glue()` or equivalent helper to implement the direct commands. Do not
deprecate or remove the direct methods as part of this effort.

### Phase 6: Centralize Proxy Construction (Complete)

Status: Complete. Private proxy constructors are controlled by `internal::proxy_factory`.

Introduce one dedicated internal factory for controlled creation of all targeted proxies:

```cpp
module_graph_proxy<void_tag> module_proxy(configuration const&);
providers_graph_proxy<void_tag> providers_proxy(configuration const&);
source_graph_proxy source_proxy(configuration const&);
resources_graph_proxy resources_proxy(configuration const&);
```

Give targeted proxies private constructors and friend the internal factory. The factory may be owned
by or created from `framework_graph`, but plugin code must not receive general factory access. Avoid
passkeys and direct friendship with macro-generated plugin functions.

### Phase 7: Encapsulate the Plugin Carrier (Complete)

Status: Complete. The mutable public aggregate was replaced by the opaque `registration_carrier`.

Replace the public aggregate `graph_registration_bundle` or reduce it to an opaque internal carrier.
It currently exposes mutable references to the graph, catalogs, resources, and registration errors.

Provider and source plugins will continue to share the current carrier and `create_source` exported
symbol shape during this refactor. Their macro-generated shim should use the internal factory to
construct the appropriate targeted proxy. This keeps loader and macro changes focused on the
structural inversion.

The carrier should not provide plugin authors with direct access to its internals. Construction and
conversion to a targeted proxy should be limited to internal factory code.

### Phase 8: Revisit the Plugin Entry Points (Complete)

Status: Complete for this refactor. Provider and source plugins continue to share the `create_source`
symbol and carrier shape; binary compatibility remains explicitly out of scope.

The current plugin interfaces use `extern "C"` linkage but pass C++ types. They do not provide a
stable C ABI. Any change to proxy or bundle layout requires rebuilding plugins.

Keep a common internal context carrier for provider and source entry points. Do not split their
creator types or loader paths as part of this work.

Compatibility with already-built plugin binaries is not required. All plugins must be rebuilt after
the proxy and carrier changes. Preserve the source-level registration macros where practical. A
versioned opaque C handle may be considered separately if independent binary compatibility later
becomes a requirement.

### Phase 9: Resolve Existing API Inconsistencies (Complete)

Status: Complete. Existing direct and proxy-specific return types, unfold ordering, output exposure,
resource registration, and configuration behavior were preserved rather than normalized.

Account for the following differences without changing the existing direct `framework_graph` API:

- `framework_graph::make<T>()` returns `glue<T>`, whereas targeted proxy `make<T>()` returns a bound
  targeted proxy.
- `framework_graph::unfold()` orders concurrency and destination-layer parameters differently from
  the current module proxy API.
- `output()` is exposed through the module proxy but not directly through `framework_graph`.
- Resource registration exists on `framework_graph` and `resources_graph_proxy` only.
- Direct registration currently omits configuration while plugin registration carries it.

Preserve these contracts during the structural inversion. Any later API normalization must be a
separate proposal and is outside this plan.

### Phase 10: Clarify Registration Lifetimes (Complete)

Status: Complete. Context and proxy lifetime constraints are documented. Python registration
wrappers are invalidated after their synchronous callbacks and reject later registration calls rather
than retaining a usable dangling proxy pointer. The build and full test suite pass with these lifetime
changes.

The loader creates configuration objects locally and invokes plugin registration synchronously.
Proxies and registration builders therefore must not escape the plugin callback.

During the refactor:

- Document this lifetime constraint on context and proxy types.
- Consider making proxies non-copyable if that does not interfere with callback use.
- Verify that Python registration wrappers cannot retain proxy pointers beyond callback completion.
- Consider framework-owned immutable configuration state if deferred registration may be supported in
  the future.

## Validation Plan

Run focused tests after each structural phase rather than waiting until the entire migration is
complete.

Required coverage includes:

- Compile-time proxy capability assertions.
- Direct `framework_graph` registration compatibility for every existing command.
- Compile-time checks for preserved `framework_graph` signatures and return types where practical.
- Module plugin registration.
- Bound module object registration.
- Explicit provider plugin registration.
- Bound provider object registration.
- Source registration.
- Resource registration.
- Output registration.
- Unfold registration and destination-stage behavior.
- Duplicate-registration error collection.
- Python module and provider registration.
- Full C++ and Python test suites.

At minimum, validate with the project's configured build and test commands:

```bash
cmake --preset default -B build
ninja -C build
ctest --test-dir build -j $(nproc) --test-timeout 90 --output-on-failure
```

Run the available pre-commit implementation against changed files before submitting the change.

The build and full test suite passed after the structural inversion and again after the Phase 10
lifetime changes.

## Compatibility Strategy

Use an incremental transition:

1. Preserve public plugin macros.
2. Preserve all existing `framework_graph` registration methods permanently.
3. Preserve capability-specific `make<T>()` return types.
4. Preserve `framework_graph::make<T>()` as a `glue<T>`-returning operation.
5. Keep direct resource registration on `framework_graph`.
6. Avoid changing registration semantics while changing object structure.
7. Treat plugin binaries as requiring a rebuild.

This separates structural risk from API and semantic risk.

## Advantages

- The capability direction is explicit: restricted facades are built directly instead of hiding an
  unrestricted base.
- `framework_graph` is the single unrestricted direct-registration interface.
- There is no redundant unrestricted proxy API to document, test, or keep synchronized.
- `framework_graph` ownership, shared registration state, and user-facing capabilities have distinct
  responsibilities.
- Direct and plugin registration can share one glue-construction implementation.
- The current `graph_proxy` forwarding layer can be removed.
- Each targeted proxy declaration shows its complete public capability set.
- Construction is enforced through private constructors and one internal factory rather than
  documentation alone.
- The design is consistent with the existing standalone `resources_graph_proxy`.
- Targeted proxies can retain only the state they require.
- Capability boundaries become easier to test and audit.

## Disadvantages and Risks

- Several internal types are introduced to represent context, bound state, and controlled creation.
- Small forwarding implementations may be repeated across sibling facades.
- `make<T>()` still requires templated state propagation and category-specific return types.
- Proxy and bundle layout changes require plugin rebuilds.
- Private construction requires an internal factory and narrowly scoped friendship.
- Preserving the distinct `framework_graph::make<T>()` contract may leave a small dedicated glue
  factory rather than eliminating all implementation overlap.
- An overly broad `registration_context` could become another unrestricted back door unless its
  access is tightly controlled.
- Registration behavior could drift between facades if they do not share glue creation.
- Existing configuration and proxy lifetime assumptions remain unless separately redesigned.
- Combining the structural refactor with signature cleanup would increase review and regression risk.

## Settled Decisions

The following choices define the implementation scope:

1. Direct `framework_graph` registration preserves its current null-configuration naming and
   validation behavior.
2. Provider and source plugins continue sharing the carrier and `create_source` exported symbol
   shape.
3. `source_graph_proxy` becomes a non-template type; macro machinery will be adapted accordingly.
4. Targeted proxies use private constructors and one dedicated internal factory.
5. Compatibility with already-built plugin binaries is not required; plugins must be rebuilt.

## Recommended Initial Scope

The first implementation should be limited to structural inversion:

1. Add capability tests.
2. Add an encapsulated registration context and common bound state.
3. Convert module, provider, and source proxies from inheritance to standalone facades.
4. Update `framework_graph::make_glue()` to use the shared state without altering its public API.
5. Remove `graph_proxy` after all targeted proxies no longer depend on it.
6. Retain direct resource commands and the existing `glue<T>`-returning `make<T>()` behavior.
7. Preserve the plugin macro source API and the shared provider/source carrier and symbol shape.
8. Run focused and full test suites.

API normalization and a stable plugin ABI should be handled as follow-up work. Removing the direct
`framework_graph` commands is not part of the target design. Keeping unrelated concerns separate
makes the inversion reviewable and reduces the chance of changing registration behavior
unintentionally.
