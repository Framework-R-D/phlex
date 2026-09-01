## 1. Model-Layer Registration Types And Concept Storage

- [x] 1.1 Add `enum class result_storage { owned, borrowed };` in the model layer, alongside the
      other translator-registration types
- [x] 1.2 Add translator storage to `data_product_concept`, keyed by the ordered pair of input and
      output `concrete_product_id`, holding a record of the translator name, the conversion
      function, and its `result_storage` value
- [x] 1.3 Reject registration when the source and target concrete types are identical (throw)
- [x] 1.4 Reject registration when either type is not already a concrete type of the concept (throw)
- [x] 1.5 Reject registration of a second translator for an already-registered ordered type pair
      (throw)
- [x] 1.6 Add an accessor returning the stored translator record (name, conversion function, and
      `result_storage` value), sufficient for this change's tests

## 2. Core Implementation

- [x] 2.1 Update `is_translator_like` in `phlex/core/concepts.hpp` to require single input and
      single output
- [ ] 2.2 Add a distinct-source-and-target constraint to `is_translator_like`
- [x] 2.3 Modify `translator_node` to deduce source type from the function's only parameter and
      target type from its return type
- [x] 2.4 Add static_assert to verify translator_node has exactly one input parameter
- [ ] 2.5 Add `product_store::stage()` accessor
- [ ] 2.6 Update `translator_node` to build the output specification from the input product's
      creator and suffix, and to construct the output store with the input's layer and stage
- [ ] 2.7 Record the translator identity on each output product
- [ ] 2.8 Add a retained-source-store member to `product_store` and set it from `translator_node`
      when the conversion was registered non-owning

## 3. Registration API

- [ ] 3.1 Change `glue::translate` to register the conversion function with a named concept instead
      of calling `make_registration<translator_node>`
- [ ] 3.2 Extend the `translate()` signature with the concept name and a `result_storage` value;
      remove input selectors and output suffixes
- [ ] 3.3 Make `graph_proxy::translate` const and take `std::string_view`, matching its siblings
- [ ] 3.4 Add `using base::translate;` to `module_graph_proxy` so it is reachable from
      `PHLEX_REGISTER_ALGORITHMS`
- [ ] 3.5 Update `framework_graph::translate` to the registration-only form

## 4. Testing

- [ ] 4.1 Test registering a conversion function with a concept creates no node and stores a record
      whose translator name, conversion function, and `result_storage` value round-trip through the
      accessor
- [ ] 4.2 Test registration is reachable from a module using `PHLEX_REGISTER_ALGORITHMS`
- [ ] 4.3 Test rejection of identical source and target types, at compile time and at the concept
- [x] 4.4 Test rejection when either type is not a concrete type of the named concept
- [x] 4.5 Test rejection of a duplicate translator for the same ordered type pair
- [ ] 4.6 Test simple type conversion (int → double) by constructing `translator_node` directly
- [ ] 4.7 Test container type conversion (`std::vector<int>` → `std::vector<double>`) by direct
      construction
- [ ] 4.8 Test conversion via a callable object or lambda
- [ ] 4.9 Test that the output product inherits creator, suffix, layer, and stage
- [ ] 4.10 Test that the output product records the translator identity
- [ ] 4.11 Test that a non-owning conversion retains the input store and an owning one does not
- [ ] 4.12 Test concurrent execution with `concurrency::unlimited`

## 5. Documentation

- [ ] 5.1 Update `translator_node` class documentation in `declared_translator.hpp`
- [ ] 5.2 Update `translate()` documentation in `glue.hpp` to describe registration-only semantics
- [ ] 5.3 Document inherited output creator, suffix, layer, stage, and distinct target type
- [ ] 5.4 Document `result_storage`, the owning/non-owning distinction, and that retention is
      whole-store
- [ ] 5.5 Document the invariants enforced by concept-level registration
- [ ] 5.6 Add a section explaining translator versus transform usage

## 6. Validation

- [ ] 6.1 Run "Build Phlex (MPD)": `spack mpd select --project phlex-work-dir && spack mpd build`
- [ ] 6.2 Run "Test Phlex (MPD)": `spack mpd select --project phlex-work-dir && spack mpd test -j 12`
