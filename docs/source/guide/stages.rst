Stages
======

A *stage name* identifies the processing stage that created a data product,
for example ``simulation``, ``digitization`` or ``reconstruction``.
Together with the creator, suffix and data layer it is part of a product's
identity.
It lets a job tell apart products that otherwise look the same but were
created by different Phlex jobs, such as a ``hits`` product read from a
simulation file and a ``hits`` product recomputed in the current job.

Choose a name that stays the same when an equivalent job is rerun.
Timestamps, host names and batch-job IDs are poor stage names.

Naming the stage of a job
-------------------------

Every job must name its stage, either with the top-level ``stage`` key or
with the ``--stage`` option, which takes precedence:

.. code-block:: jsonnet

   {
     stage: 'reconstruction',
     driver: { /* ... */ },
     modules: { /* ... */ },
   }

.. code-block:: console

   phlex -c workflow.jsonnet --stage reconstruction

A job fails at startup if no stage is given, if it is empty, or if it is
``"CURRENT"``, which is reserved (see below).
The stage requirement was introduced in Phlex v0.4.0; configurations written
for earlier versions need a ``stage`` key added.

Stages of created products
--------------------------

Every product created by a transform, fold or unfold in the job carries the
job's stage.

Providers registered with ``PHLEX_REGISTER_PROVIDERS`` declare their
product with ``output_product(creator, suffix, layer, stage)``.
The ``stage`` argument defaults to ``"CURRENT"``, which Phlex replaces by the
job's stage.
A provider that reads products written by an earlier job can instead pass
that job's stage name, so that its products keep their original identity.

Implicit providers, which a ``phlex::source`` returns from
``create_providers()``, must set ``provider_bundle::stage`` to a real stage
name.
Returning ``"CURRENT"`` is an error, because no substitution takes place for
implicit providers.

.. note::

   In Phlex v0.4 a ``phlex::source`` has no access to the job's stage.
   A source that creates new products, such as an event generator, must take
   the stage name from its own configuration block, and the workflow has to
   keep that value consistent with the job's stage.
   If the job's stage is then changed with ``--stage``, the source's products
   keep the old name.
   This is tracked in
   `issue #957 <https://github.com/Framework-R-D/phlex/issues/957>`_.

Selecting products by stage
---------------------------

``product_selector`` has an optional ``stage`` field; in a configuration
file a selector object accepts a ``stage`` key.

Not set
   Products of any stage match.

``"CURRENT"``
   Only products of the job's own stage match.
   This substitution is not applied for sources: the selector reaches
   ``create_providers()`` with the literal ``"CURRENT"``, which a source that
   checks candidates with ``selector.match(spec, layer, stage)`` will not
   match.
   To select such a product, leave ``stage`` unset or name the stage
   explicitly.

Any other name
   Only products of that stage match.
   If it is not the job's own stage, Phlex looks for the product only among
   providers, because everything computed in the job carries the job's stage.

.. code-block:: cpp

   m.transform("refit", refit, concurrency::unlimited)
     .input_family(product_selector{
       .creator = "tracks", .layer = "event", .stage = "reconstruction"});

Selectors written in Python cannot set ``stage`` yet.

An algorithm that takes a ``phlex::handle<T>`` can read the stage of the
product it received with ``handle::stage()``.
