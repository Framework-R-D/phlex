Unfolds
=======

An *unfold* splits one data cell into child data cells in a new, lower
data layer: for example, a spill into the events it contains, or an event
into track seeds.
Each child gets one product, which downstream algorithms select in the
child layer.

Registering an unfold
---------------------

An unfold is described by a class (the *unfolder*) and two functions:

.. code-block:: cpp

   class VectorSplitter {
   public:
     explicit VectorSplitter(std::vector<int> const& values) : values_{values} {}

     std::size_t initial_value() const { return 0; }
     bool predicate(std::size_t i) const { return i < values_.size(); }
     std::pair<std::size_t, int> unfold(std::size_t i) const { return {i + 1, values_[i]}; }

   private:
     std::vector<int> const& values_;
   };

   PHLEX_REGISTER_ALGORITHMS(m)
   {
     using namespace phlex;

     m.unfold<VectorSplitter>("split",
                              &VectorSplitter::predicate,
                              &VectorSplitter::unfold,
                              "number",                 // child layer
                              concurrency::unlimited)
       .input_family(product_selector{.creator = "input", .layer = "spill", .suffix = "numbers"})
       .output_product_suffixes("number");

     m.transform("square", [](int n) { return n * n; }, concurrency::unlimited)
       .input_family(product_selector{.creator = "split", .layer = "number", .suffix = "number"})
       .output_product_suffixes("squared");
   }

For each parent data cell, Phlex:

#. constructs a ``VectorSplitter`` from the products selected by
   ``input_family(...)``, passed to the constructor in order;
#. sets a running value to ``initial_value()``;
#. while ``predicate(value)`` returns ``true``, calls ``unfold(value)``,
   which returns the next running value and the product for a new child
   data cell.

Phlex deduces the unfolder's inputs from its constructor parameters, one
per selector.
Give the unfolder exactly one constructor: if it also has a default or a
shorter constructor, Phlex picks that one, and registration fails to
compile.

The predicate and unfold functions may be member-function pointers of the
unfolder, or callables that take the unfolder as their first argument, or
callables that take only the running value.
The unfold function may also take the new child's ``data_cell_index``
after the running value.

The child layer does not need to be declared in the driver configuration;
it is created by the unfold.
Its products have the unfold's name as creator and the suffix given to
``output_product_suffixes(...)``.

Behaviour to be aware of
------------------------

One call per parent
   The whole loop for one parent runs inside a single call of the node, and
   the ``concurrency`` argument limits how many parents are unfolded at the
   same time.
   Children are passed downstream as they are created, so downstream
   algorithms can start before the loop finishes.

Children are numbered from 0
   Child data cells are numbered 0, 1, 2, ... separately for each parent.

No children is allowed
   If the predicate returns ``false`` for the initial value, the parent has
   no children, and child-layer algorithms are not called for it.

No skipping
   Every successful predicate call creates a child.
   To leave out candidates, advance the running value past them inside the
   unfolder (for example, store "index of the next accepted candidate" as
   the running value), rather than creating a child and discarding it later.

Products may refer to the inputs
   The unfolder is constructed and used within one call, while its input
   products are alive, so it may keep references to them, as above.
   The child products themselves must not refer to the unfolder.

Input layers
------------

Every input selector of an unfold must name a data layer, and all of them
must name the *same* layer.
Since Phlex v0.4.3, an unfold whose inputs come from different layers (for
example, a spill-layer product and a run-layer product) is rejected with an
``std::invalid_argument`` exception, ``Unfolds cannot yet accept multi-layer
inputs``, before any data is processed.
Earlier versions accepted such unfolds but could leave the higher layer
unflushed, so that folds over it never produced results
(`issue #955 <https://github.com/Framework-R-D/phlex/issues/955>`_).
Support is planned for Phlex v0.5.

Until then, combine the products in a transform on the parent layer and
unfold its single output:

.. code-block:: cpp

   // spill-layer transform that bundles what the unfolder needs
   m.transform("prepare", prepare, concurrency::unlimited)
     .input_family(product_selector{.creator = "hits", .layer = "spill"},
                   product_selector{.creator = "geometry", .layer = "job"})
     .output_product_suffixes("context");

   m.unfold<SeedFinder>("seeds", &SeedFinder::more, &SeedFinder::next, "seed")
     .input_family(product_selector{.creator = "prepare", .layer = "spill", .suffix = "context"});

Algorithms downstream of an unfold may combine child-layer products with
products from the parent or higher layers.

Resources
---------

``resource<T>{}`` arguments given after the selectors in
``input_family(...)`` are passed to the unfold function after its other
arguments, not to the unfolder's constructor
(`issue #946 <https://github.com/Framework-R-D/phlex/issues/946>`_).
