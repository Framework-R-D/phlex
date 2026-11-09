Providers and sources
=====================

Data products that no algorithm in the job creates, such as products read
from a file or generated events, enter the graph through *providers*.
A provider is called once for each data cell of its layer and returns the
product for that data cell.
Providers run only when some algorithm needs their product.

There are two ways to supply providers.
Both kinds of plugin are configured in the top-level ``sources`` table.

Explicit providers
------------------

A plugin with a ``PHLEX_REGISTER_PROVIDERS`` entry point (from
``phlex/source.hpp``) registers one provider per product:

.. code-block:: cpp

   #include "phlex/configuration.hpp"
   #include "phlex/source.hpp"

   PHLEX_REGISTER_PROVIDERS(m, config)
   {
     auto const layer = config.get<std::string>("layer");

     m.provide("provide_i", [](phlex::data_cell_index const& id) -> int { return id.number(); })
       .output_product("input", "i", phlex::experimental::identifier{layer});
   }

``output_product(creator, suffix, layer, stage)`` sets the identity of the
product: algorithms select it with ``.creator = "input"`` and ``.suffix =
"i"`` in that layer.
``stage`` defaults to the job's stage.

Sources and implicit providers
------------------------------

A *source* does not supply products itself: it is a factory for providers.
At graph-construction time it decides which products its providers can
supply and creates an implicit provider for each.
It derives from ``phlex::source`` and is registered with
``PHLEX_REGISTER_SOURCE``:

.. code-block:: cpp

   #include "phlex/configuration.hpp"
   #include "phlex/source.hpp"

   class tracks_source : public phlex::source {
   public:
     tracks_source(std::string file, std::string stage) :
       file_{std::move(file)}, stage_{std::move(stage)} {}

     phlex::provider_bundles create_providers(phlex::product_selector const& selector) override
     {
       using namespace phlex::experimental;
       std::string const layer = "event";
       product_specification spec{"reader", "tracks", make_type_id<std::vector<Track>>()};
       if (not selector.match(spec, identifier{layer}, identifier{stage_})) {
         return {};
       }
       return {{.provider_function = [this](phlex::data_cell_index const& id) {
                  return product_for(read_tracks(id.number()));
                },
                .max_concurrency = phlex::concurrency::serial,
                .spec = std::move(spec),
                .layer = layer,
                .stage = stage_}};
     }

   private:
     std::vector<Track> read_tracks(std::size_t event) const;
     std::string file_;
     std::string stage_;
   };

   PHLEX_REGISTER_SOURCE(s, config)
   {
     s.add_source<tracks_source>(config.get<std::string>("module_label"),
                                 config.get<std::string>("file"),
                                 config.get<std::string>("stage"));
   }

The arguments after the name in ``add_source<T>(name, args...)`` are
passed to the source's constructor.
The name must be unique in the job (registering a second source under the
same name fails), and it is what a driver's ``uses_sources`` list refers to.
Using the entry's ``module_label``, as above, keeps it unique when the
plugin is configured more than once and makes it the same as the label in
the ``sources`` table.

How ``create_providers`` is called
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While building the graph, Phlex collects every algorithm input that no
algorithm and no explicit provider creates.
For each such input that an implicit provider created earlier does not
already serve, it calls ``create_providers()`` on every source, passing the
input's product selector.
A source returns a ``provider_bundle`` for each product it can supply that
matches the selector, or an empty vector.

- The selector's fields may be unset: depending on what the algorithm asked
  for, it may have no creator, suffix, layer or stage.
  Use ``selector.match(spec, layer, stage)`` to test a candidate product,
  instead of reading the fields directly.
  Converting an unset ``selector.layer`` to an identifier throws
  ``std::logic_error``.
- At most one bundle may match a selector across all sources; otherwise
  graph construction fails with ``Multiple implicit providers found``.
- Each provider node is named after the creator in its bundle's
  ``spec``.
  Two bundles with the same creator but different products therefore
  collide, and graph construction fails with ``Implicit providers not yet
  supported for creators that created multiple data products``.
  Give each product a source supplies its own creator name.
- A selector with ``.stage = "CURRENT"`` is passed unchanged, so a source
  that matches on the stage never matches it.
- A bundle's ``stage`` must be a real stage name.
  ``"CURRENT"`` is rejected, and the source is not told the job's stage
  (`issue #957 <https://github.com/Framework-R-D/phlex/issues/957>`_), so
  take it from the source's configuration, as above.

Each bundle becomes a separate provider node with its own
``max_concurrency``.
Providers returned by the same source can therefore run at the same time
as each other, even if each is ``concurrency::serial``.
If they share state, such as an open file, protect it inside the source.

``source::indices()`` is not called by the framework.
A driver that lists the source in its ``uses_sources`` configuration can
call it to learn which data cells the source holds.
Overriding it is optional.

Provisional types
~~~~~~~~~~~~~~~~~

``provider_bundle`` and ``provider_bundles`` are in the ``phlex`` namespace.
The types used to fill a bundle (``product_specification``,
``make_type_id``, ``product_for``, ``product_ptr`` and ``provider_function``)
are in ``phlex::experimental`` and may change before they are promoted.
Code written for Phlex v0.3 that spells them with ``phlex::detail`` must be
updated.
