## 1. Research

- [ ] 1.1 Research how type traits could be leveraged in the translator_node implementation
- [ ] 1.2 Evaluate whether type traits provide meaningful flexibility over direct type deduction
- [ ] 1.3 Document findings in design.md (or note in tasks if no change needed)

## 2. Concept-Level Translator Storage

- [ ] 2.1 Add translator-function storage to `data_product_concept`, keyed by the ordered pair of
      input and output `concrete_product_id`
- [ ] 2.2 Reject registration when the source and target concrete types are identical (throw)
- [ ] 2.3 Reject registration when either type is not already a concrete type of the concept (throw)
- [ ] 2.4 Reject registration of a second translator for an already-registered ordered type pair
      (throw)
- [ ] 2.5 Add an accessor for a stored translator function, sufficient for this change's tests

## 3. Core Implementation

- [x] 3.1 Update `is_translator_like` in `phlex/core/concepts.hpp` to require single input and
      single output
- [ ] 3.2 Add a distinct-source-and-target constraint to `is_translator_like`
- [x] 3.3 Modify `translator_node` to deduce source type from the function's only parameter and
      target type from its return type
- [x] 3.4 Add static_assert to verify translator_node has exactly one input parameter
- [ ] 3.5 Add `product_store::stage()` accessor
- [ ] 3.6 Update `translator_node` to build the output specification from the input product's
      creator and suffix, and to construct the output store with the input's layer and stage
- [ ] 3.7 Record the translator identity on each output product
- [ ] 3.8 Add a retained-source-store member to `product_store` and set it from `translator_node`
      when the conversion was registered as aliasing
- [ ] 3.9 Remove any source/target equality checking from `translator_node` (enforced at the
      concept)

## 4. Registration API

- [ ] 4.1 Change `glue::translate` to register the conversion function with a named concept instead
      of calling `make_registration<translator_node>`
- [ ] 4.2 Extend the `translate()` signature with the concept name and the aliasing indication;
      remove input selectors and output suffixes
- [ ] 4.3 Make `graph_proxy::translate` const and take `std::string_view`, matching its siblings
- [ ] 4.4 Add `using base::translate;` to `module_graph_proxy` so it is reachable from
      `PHLEX_REGISTER_ALGORITHMS`
- [ ] 4.5 Update `framework_graph::translate` to the registration-only form

## 5. Testing

- [ ] 5.1 Test registering a conversion function with a concept stores it and creates no node
- [ ] 5.2 Test registration is reachable from a module using `PHLEX_REGISTER_ALGORITHMS`
- [ ] 5.3 Test rejection of identical source and target types, at compile time and at the concept
- [ ] 5.4 Test rejection when either type is not a concrete type of the named concept
- [ ] 5.5 Test rejection of a duplicate translator for the same ordered type pair
- [ ] 5.6 Test simple type conversion (int → double) by constructing `translator_node` directly
- [ ] 5.7 Test container type conversion (`std::vector<int>` → `std::vector<double>`) by direct
      construction
- [ ] 5.8 Test conversion via a callable object or lambda
- [ ] 5.9 Test that the output product inherits creator, suffix, layer, and stage
- [ ] 5.10 Test that the output product records the translator identity
- [ ] 5.11 Test that an aliasing conversion retains the input store and a non-aliasing one does not
- [ ] 5.12 Test concurrent execution with `concurrency::unlimited`

## 6. Documentation

- [ ] 6.1 Update `translator_node` class documentation in `declared_translator.hpp`
- [ ] 6.2 Update `translate()` documentation in `glue.hpp` to describe registration-only semantics
- [ ] 6.3 Document inherited output creator, suffix, layer, stage, and distinct target type
- [ ] 6.4 Document the aliasing indication and that retention is whole-store
- [ ] 6.5 Document the invariants enforced by concept-level registration
- [ ] 6.6 Add a section explaining translator versus transform usage

## 7. Validation

- [ ] 7.1 Run "Build Phlex (MPD)": `spack mpd select --project phlex-work-dir && spack mpd build`
- [ ] 7.2 Run "Test Phlex (MPD)": `spack mpd select --project phlex-work-dir && spack mpd test -j 12`
