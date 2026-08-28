## Context

The Phlex framework uses TBB flow graphs with specialized node types for data processing:

- `transform_node`: General-purpose transformations (1+ inputs → 1+ outputs)
- `translator_node`: Currently behaves like transform_node but conceptually distinct
- `provider_node`: Produces data from indices
- `predicate_node`: Filters messages

The existing `translator_node` is a renamed copy of `transform_node`. It was created only as a
skeleton of code to be modified; its current behavior is not at all what is wanted.

### Translator Node Behavior

- Translator nodes convert one input concrete type to a distinct output concrete type
- The conversion function supplies the input and output types through its signature
- The output product preserves the input product's creator, suffix, layer, and stage
- The output product records the translator algorithm identity

### Invariants established elsewhere

A conversion function can enter the system only by being registered with a `data_product_concept`.
That single chokepoint enforces that the source and target types model the same concept and that
they are distinct. `translator_node` consumes functions that already satisfy these invariants and
re-checks neither.

## Goals / Non-Goals

**Goals:**

1. Simplify type conversion registration API for the common use case
2. Deduce types from the function signature to reduce boilerplate
3. Ensure type safety at registration time
4. Preserve product identity through type conversion
5. Maintain compatibility with the TBB graph execution model

**Non-Goals:**

- Do not change `transform_node` behavior
- Do not add the `data_product_concept_registry` interface for looking up translator functions
- Do not integrate translator lookup into graph building; automatic translator discovery and node
  placement are a separate change
- Do not modify the core TBB execution engine
- Do not change plugin loading or the module system

## Decisions

### 1. Type Deduction Strategy

**Decision**: Deduce the source type from the function's only parameter, and the target type from
its return type.

**Rationale**:

- Mirrors user intent: `TargetType convert(SourceType const&)` is the canonical pattern
- Reduces API surface: no need to specify types explicitly
- Leverages existing type deduction infrastructure (`type_deduction.hpp`)

**Alternatives considered**:

- Explicit template parameters: would require users to specify types twice (function signature plus
  template arguments)
- Type traits: should be considered if they add flexibility or simplify the design

### 2. Output Product Specification

**Decision**: The output product preserves the input product's metadata:

- Same creator name (from the input product)
- Same suffix (from the input product)
- Same layer and stage (from the input product)
- Concrete type changed to the target type deduced from the function's return type

**Rationale**:

- Type conversion is a 1:1 mapping of the same conceptual data
- The translator is a pass-through for identification metadata
- Only the concrete type changes, not the logical identity
- Once translator nodes are placed by the graph builder, a consumer's selector must be satisfied by
  the translator's output exactly as it would have been by the original producer's; inheriting
  creator, suffix, layer, and stage is what makes the substitution invisible

**Implementation approach**: Construct the output product specification from the input product's
creator and suffix, substituting only the deduced target type; take layer from the input store's
index and stage from the input store.

Today `translator_node` builds its output specification from its own node name via
`to_product_specifications(name(), ...)` and constructs the output store without a stage argument,
so both must change.

**Alternatives considered**:

- Keep the vector API: incorrect for `translator_node`
- Auto-generate a suffix from type names: would be opaque to users

### 3. Registration Is Separate From Node Creation

**Decision**: `translate()` registers a conversion function with a named data-product concept and
creates no graph node.

**Rationale**:

- A conversion function such as `double convert(int const&)` is generic: it applies to any `int`
  product regardless of creator or layer
- Creating a node at registration time would require a `product_selector`, whose `layer` field is
  mandatory, forcing one registration per (creator, layer) pair and destroying that genericity
- Node placement belongs to the graph builder, which knows which consumer inputs are unsatisfied

**Implementation approach**: `glue::translate` stops calling `make_registration<translator_node>`
and instead records the function with the concept. `framework_graph::translate` follows.
`module_graph_proxy` must add `translate` to its `using base::...` list, and
`graph_proxy::translate` must become `const` and take `std::string_view` to match its siblings, so
that it is callable on the `const&` proxy passed to a module.

**Consequence**: no public API constructs a `translator_node` in this change. Node behavior is
verified by tests that construct `translator_node` directly against a TBB graph.

### 4. Concurrency Support

**Decision**: Retain the existing concurrency model (serial/unlimited/custom).

**Rationale**:

- No new execution semantics are needed
- Conversion operations are independent
- Consistent with other node types

### 5. Provenance Metadata

**Decision**: Each output product records the identity of the translator algorithm that produced
it, and nothing more.

**Rationale**:

- Enables tracking of data product lineage through the graph
- Supports FORM I/O and product identification
- Required by translator node semantics (see `design_wiki/seeds/the-nature-of-translator-nodes.md`
  in the separate `phlex-design` repo, items 9 and 11)

**Implementation approach**: Add a translator-identity field to the output product model and
populate it when `translator_node` creates each output product.

**Note**: an earlier version of this decision also recorded a non-owning reference to the input
*product*. That is not representable: a store owns its products by value through `unique_ptr`
elements, so an individual product cannot be referenced or shared across stores. Where a reference
to the source is needed for data validity, it is handled at store granularity by Decision 6.

### 6. Source Retention For Non-Owning Conversions

**Decision**: Registration declares, through a `result_storage` value, whether a conversion is
*owning* (its result owns its storage) or *non-owning* (its result refers to storage owned by the
input product). For a non-owning conversion, the output store retains the input store.

**Rationale**:

- A conversion may legitimately return a view over the source (a span, a pointer-carrying type),
  in which case the result dangles if the source is released
- This cannot be deduced: `std::span` is detectable, but a user-defined type holding a pointer into
  the source is not, so it must be declared
- This is a correctness precondition, not a tuning knob: registering a non-owning conversion as
  owning is a defect the framework cannot detect
- Ordinary owning conversions do not pin stores, so retention costs nothing in the common case

**Naming**: "owning" and "non-owning" describe the result's relationship to its storage, which is
what the registrant actually knows and what determines the lifetime obligation. The earlier term
"aliasing" was avoided because in C++ it is preloaded with type-based alias analysis and strict
aliasing, neither of which is meant here.

**Interface**: the declaration is a two-valued enum, not a `bool`:

```cpp
enum class result_storage { owned, borrowed };
```

A call site reads `result_storage::borrowed` rather than a bare `true`, which matters because
misdeclaring the value is undetectable by the framework. The enumerators name the storage of the
result; `owned` is the ordinary case.

**Implementation approach**: Add a retained-source-store member of type `product_store_const_ptr`
to `product_store`, set by `translator_node` when the conversion was registered non-owning. This
mirrors `unfold_node`, which already holds its parent store alive.

**Granularity**: retention is necessarily whole-store. Products are owned by value inside their
store and only the store is shareable, so retaining the referenced product retains every product in
its store. A chain of non-owning translators retains a chain of stores.

This mechanism is independent of Decision 5: provenance is informational and always present;
retention is about data validity and is conditional.

### 7. Invariant Enforcement At The Concept

**Decision**: `data_product_concept` gains storage for translator functions, keyed by the ordered
pair of input and output `concrete_product_id`. It rejects a registration when:

- the source and target types are identical
- either type is not already a concrete type of that concept
- a translator is already registered for that ordered pair

Rejection throws, matching `register_concept`'s handling of a null concept.

**Rationale**:

- Every translator function passes through this one point, so invariants enforced here hold for the
  whole system and need not be re-checked by `translator_node`
- Rejecting duplicates keeps graph construction deterministic: module load order is not guaranteed,
  so silently choosing between two converters for the same pair would make behavior depend on load
  order
- Throwing aborts module loading rather than silently skipping a registration the author believed
  had taken effect

**Also enforced at compile time**: `is_translator_like` gains a distinct-types constraint, so the
identical-type mistake is caught when registering through the typed interface rather than at module
load. The concept-level check remains for type-erased callers.

### 8. Backward Compatibility

**Decision**: This is a BREAKING change.

**Rationale**:

- `translator_node` has no users yet (experimental feature)
- Future users benefit from the simplified API
- No migration is needed

## Risks / Trade-offs

### [Risk] Type deduction might fail for complex signatures

**Mitigation**: Provide clear error messages through compile-time constraints and static
assertions.

### [Risk] Requiring prior type membership makes registration order-dependent

Both converted types must already be concrete types of the named concept, but module load order is
not guaranteed, so a module registering a translator may load before the module that adds those
types to the concept.

**Mitigation**: Report the failure with the concept name and the offending type so the cause is
unambiguous. If ordering proves burdensome in practice, revisit whether translator registration
should add missing types.

### [Risk] Non-owning conversions pin whole stores

Retention is whole-store, so a non-owning translator keeps every product in its input store alive,
and chained non-owning translators keep a chain alive.

**Mitigation**: Confine retention to conversions declared non-owning, and document the granularity
so registrants can prefer owning conversions where practical.

### [Risk] No public creation path in this change

Because registration no longer creates nodes and builder-driven placement is deferred, node
behavior is only exercised by direct construction in tests.

**Mitigation**: Cover conversion, identity inheritance, provenance, and retention by direct
construction so the follow-up change inherits a tested node.

## Migration Plan

1. Add translator-function storage and its invariants to `data_product_concept`
2. Implement changes to `translator_node`, `product_store`, and the registration path
3. Update documentation for `translate()`

## Open Questions

### How are translator functions represented once the registry hands them to node construction?

`translator_node` is a template whose types are deduced at compile time, but concept storage is
keyed by `std::type_index` and carries no static type information. The follow-up change must decide
whether the concept stores type-erased factories that instantiate the correct
`translator_node<AlgorithmBits>` specialization, or whether `translator_node` becomes a non-template
holding a type-erased callable plus run-time type ids. This choice does not block the present
change, which stores functions but does not look them up.
