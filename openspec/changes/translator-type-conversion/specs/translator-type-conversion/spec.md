## Purpose

Defines how a conversion function between two concrete types is registered with a data-product
concept, and how a translator node converts a product of one concrete type into a product of a
distinct concrete type while preserving the product's identity.

## ADDED Requirements

### Requirement: Translator node performs type conversion

A translator node SHALL convert an input object of one concrete type into an output object of a
distinct concrete type. The source type is deduced from the conversion function's only parameter.
The target type is deduced from the conversion function's return type.

#### Scenario: Type conversion with simple types

- **WHEN** a conversion function `double convert(int const& value)` is used to build a translator
- **THEN** the node accepts `int` products as input and produces `double` products as output

#### Scenario: Type conversion with container types

- **WHEN** a conversion function `std::vector<double> convert(std::vector<int> const& input)` is
  used to build a translator
- **THEN** the node accepts `std::vector<int>` products as input and produces
  `std::vector<double>` products as output

#### Scenario: Type conversion via callable object

- **WHEN** a callable object, such as a lambda, defines `TargetType operator()(SourceType const&) const`
- **THEN** the source and target types are deduced from that call operator

### Requirement: Single input and single output

A translator node SHALL accept exactly one input product and produce exactly one output product.

#### Scenario: Conversion function with more than one parameter

- **WHEN** a conversion function declares more than one parameter
- **THEN** the translator is rejected at compile time

#### Scenario: Single output produced per conversion

- **WHEN** a translator node completes a conversion
- **THEN** it produces exactly one output product

### Requirement: Output product preserves input product identity

A translator node SHALL create an output product that preserves the input product's creator,
suffix, layer, and stage. Only the concrete type differs.

#### Scenario: Identity is inherited

- **WHEN** a translator converts a product whose creator is "source_node", suffix is "data", layer
  is "event", and stage is "CURRENT"
- **THEN** the output product has creator "source_node", suffix "data", layer "event", and stage
  "CURRENT"
- **AND** the output product has the concrete target type deduced from the conversion function

#### Scenario: Caller does not supply an output suffix

- **WHEN** a translator is registered
- **THEN** no output suffix is accepted from the registrant, because the suffix is inherited from
  the input product

### Requirement: Output product records the translator identity

A translator node SHALL record, on each output product, the identity of the translator that
produced it.

#### Scenario: Translator identity is recorded

- **WHEN** a translator node named "int_to_double" completes a conversion
- **THEN** the output product records "int_to_double" as the translator that produced it

### Requirement: Conversions that alias input storage retain the input store

Registration SHALL accept an indication of whether the conversion's result refers to storage owned
by the input product. When so indicated, the output product's store SHALL retain the input store
for at least as long as the output store is alive.

#### Scenario: Aliasing conversion retains its source

- **WHEN** a conversion is registered as producing a result that refers into the input product's
  storage
- **THEN** the store holding the output product retains the store holding the input product
- **AND** every product in the retained input store stays alive for as long as the output store

#### Scenario: Non-aliasing conversion does not retain its source

- **WHEN** a conversion is registered as producing a self-contained result
- **THEN** the store holding the output product does not retain the input store

### Requirement: Conversion functions are registered with a data-product concept

A conversion function SHALL be registered with a named data-product concept, together with the
translator name and the aliasing indication. Registration SHALL NOT create a graph node.

#### Scenario: Successful registration

- **WHEN** a module registers a conversion function between two concrete types that are both
  concrete types of the named concept, and no translator is yet registered for that pair
- **THEN** the conversion function is stored with that concept, keyed by its input and output
  types
- **AND** no graph node is created

#### Scenario: Registration is reachable from a module

- **WHEN** a module is written using the algorithm registration macro
- **THEN** the registration interface it receives exposes the translator registration operation

### Requirement: Identical source and target types are rejected

Registration SHALL reject a conversion function whose source and target concrete types are the
same.

#### Scenario: Identical types rejected at compile time

- **WHEN** a conversion function with signature `T convert(T const&)` is registered through the
  typed registration interface
- **THEN** the registration fails to compile with a message identifying the identical types

#### Scenario: Identical types rejected by the concept

- **WHEN** a conversion function whose source and target types are identical reaches the concept's
  translator storage
- **THEN** the registration is rejected with an error identifying the identical types

### Requirement: Both converted types must model the named concept

Registration SHALL reject a conversion function unless both its source and its target type are
already concrete types of the named data-product concept.

#### Scenario: Type not yet a member of the concept

- **WHEN** a conversion function is registered with a concept for which the source or the target
  type is not a registered concrete type
- **THEN** the registration is rejected with an error naming the concept and the offending type

### Requirement: Duplicate translators for a type pair are rejected

A data-product concept SHALL hold at most one conversion function for a given ordered pair of
source and target concrete types.

#### Scenario: Second translator for the same pair

- **WHEN** a conversion function is registered for an input and output type pair that already has
  a registered conversion function in that concept
- **THEN** the registration is rejected with an error naming the concept and the two types

### Requirement: Concurrent execution support

Translator nodes SHALL support the same concurrency models as other node types (serial, unlimited,
custom).

#### Scenario: Concurrent type conversion

- **WHEN** a translator node is built with `concurrency::unlimited`
- **THEN** multiple conversion operations may execute in parallel
