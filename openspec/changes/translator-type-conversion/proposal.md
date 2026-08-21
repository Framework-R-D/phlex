## Why

The `translator_node` class is currently a renamed copy of `transform_node` with no specific
semantics for type conversion. It was created only as a skeleton of code to be modified.

Users need a way to convert objects from one concrete type to another (e.g.,
`std::vector<int>` → `std::vector<double>`). The existing `transform_node` supports arbitrary
transformations with potentially multiple inputs and outputs, which is more complex than needed
for simple type conversions.

Conversion functions belong to a data-product concept: a concept groups the concrete types that
model it, and a translator function converts between two of those types. A conversion function can
enter the system only by being registered with a concept, so every translator in the system
satisfies two invariants by construction:

- its input and output types model the same data-product concept
- its input and output types are distinct

`translator_node` therefore performs neither check.

This change:

- Modifies `translator_node` to perform one-to-one type conversion, deducing source and target
  types from the conversion function's signature
- Preserves the input product's creator, suffix, layer, and stage on the output product, changing
  only the concrete type
- Records the translator identity on the output product
- Adds translator-function storage to `data_product_concept`, which is where the above invariants
  are enforced
- Turns `translate()` into a registration-only operation: it records a conversion function with a
  named concept and creates no graph node

Registration must supply the translator name, the conversion function, the concept modeled by the
input and output types, and whether the conversion's result aliases the input product's storage.

## What Changes

- Modify `translator_node` to perform one-to-one type conversions
  - Source type deduced from the function's only parameter; target type from its return type
  - Output product preserves the input product's creator, suffix, layer, and stage
  - Output product records the translator identity
  - Output store optionally retains the input store when the conversion aliases input storage
- Add translator-function storage to `data_product_concept`, keyed by the input and output
  `concrete_product_id`, which rejects a registration when
  - the source and target types are identical
  - either type is not already a concrete type of the named concept
  - a translator is already registered for that input/output pair
- Change `translate()` on the graph proxy and `framework_graph` to register a conversion function
  with a concept rather than create a node
- Expose `translate` on `module_graph_proxy` so it is reachable from `PHLEX_REGISTER_ALGORITHMS`
- Add a `stage()` accessor and a retained-source-store member to `product_store`
- Update the `is_translator_like` concept to require distinct source and target types, so the
  identical-type error is also caught at compile time

**BREAKING**: `translate()` no longer creates a graph node and no longer accepts input selectors
or output suffixes. It now expects a conversion function with signature
`TargetType convert(SourceType const&)`, a concept name, and an aliasing indicator.

No public API creates a `translator_node` in this change; nodes are created by the graph builder
in a follow-up change. Node behavior is verified by tests that construct `translator_node`
directly.

## Capabilities

### New Capabilities

- **translator-type-conversion**: Type conversion behavior for `translator_node` and the
  registration of conversion functions with a data-product concept

### Modified Capabilities

None

## Impact

**Affected files:**

- `phlex/core/declared_translator.hpp`: `translator_node` class implementation
- `phlex/core/declared_translator.cpp`: Base class implementation
- `phlex/core/concepts.hpp`: `is_translator_like` concept
- `phlex/core/glue.hpp`: `translate()` becomes registration-only
- `phlex/core/graph_proxy.hpp`: `translate()` signature
- `phlex/module.hpp`: expose `translate` on `module_graph_proxy`
- `phlex/core/framework_graph.hpp`: `translate()` registration method
- `phlex/model/data_product_concept.hpp/.cpp`: translator-function storage and its invariants
- `phlex/model/product_store.hpp/.cpp`: `stage()` accessor, retained source store

**API changes:**

- Registrants provide conversion functions with explicit source→target signatures, the concept
  name, and whether the result aliases input storage
- Both the source and target types must already be registered as concrete types of the named
  concept before a translator between them can be registered
- Output product inherits creator, suffix, layer, and stage from the input product; callers do not
  specify an output suffix
- Output type is the distinct concrete target type deduced from the conversion function
- Output products record the translator identity

**Memory behavior:**

- Products are owned by their store and cannot be retained individually. When a conversion is
  declared to alias input storage, the output store retains the **entire** input store, keeping
  every product in it alive for as long as the output lives. Chained aliasing translators retain a
  chain of stores.

**Non-goals:**

- Not changing `transform_node` behavior
- Not modifying the underlying TBB graph execution mechanics
- Not changing the plugin system or module loading
- Not adding the `data_product_concept_registry` interface for looking up translator functions
- Not integrating translator lookup into graph building, and not implementing automatic translator
  discovery or node placement
