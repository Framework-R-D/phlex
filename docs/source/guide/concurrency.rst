Concurrency and scheduling
==========================

Phlex runs each registered algorithm as a node of a oneTBB flow graph.
Nodes run as soon as their inputs are available, on the threads of a single
oneTBB thread pool.
This page describes what Phlex does and does not guarantee about when and
where an algorithm is called.

Concurrency limits
------------------

Every registration function takes a ``phlex::concurrency`` argument that
limits how many calls of that algorithm may run at the same time:

``concurrency::serial``
   At most one call at a time.
   This is the default for every registration function (``transform``,
   ``fold``, ``unfold``, ``observe``, ``predicate``, ``provide`` and
   ``output``).
   Implicit providers take their limit from
   ``provider_bundle::max_concurrency``, which has no default: a bundle that
   leaves it out is unlimited, not serial.

``concurrency::unlimited``
   No limit other than the number of threads available to the job.

``concurrency{n}``
   At most ``n`` calls at a time.
   ``concurrency{0}`` is the same as ``concurrency::unlimited``.

.. code-block:: cpp

   m.transform("digitize", digitize, concurrency::unlimited)
     .input_family(product_selector{.creator = "hits", .layer = "event"});

The default may change in a later version
(`issue #949 <https://github.com/Framework-R-D/phlex/issues/949>`_), so give
the concurrency explicitly where it matters.

What a concurrency limit does not guarantee
-------------------------------------------

A concurrency limit only bounds the number of simultaneous calls of one
node.
In particular:

No ordering
   Calls are not made in order of data-cell number, even for a serial node,
   and even when the driver creates data cells in order.
   An algorithm that keeps sequential state, such as a single random-number
   stream, must not assume that data cell *n* is processed before data cell
   *n + 1*.
   Derive per-cell state from the ``data_cell_index`` instead (for example,
   seed a counter-based generator with the data-cell number), or buffer and
   reorder inside the algorithm.

No thread affinity
   A call may run on any thread of the pool, and consecutive calls of a
   serial node may run on different threads.
   Libraries that keep thread-local state, or that must only be used from
   the thread that initialized them, need their own arrangement, such as
   forwarding the work to a dedicated thread.

No exclusion between nodes
   The limit is per node.
   Two serial algorithms registered on the same bound object (through
   ``make<T>()``) may run at the same time.
   So may the providers that one ``phlex::source`` returns, because each
   becomes a separate node.
   State shared between nodes needs its own protection, such as a mutex or a
   serialized resource.

Folds share their accumulator
   A fold registered with a limit above one may call the fold function
   for several data cells at the same time on the *same* accumulator object.
   The accumulator must then be safe to update concurrently, for example a
   ``std::atomic``.

No output order
   Observers and output modules receive data cells in the order processing
   finishes.
   Record the data-cell number with the data if it is needed later.

Number of threads
-----------------

The size of the thread pool is set by ``-j`` or the ``max_concurrency``
configuration key, and defaults to oneTBB's default concurrency.
Phlex installs this limit as a ``tbb::global_control`` before any plugin is
loaded, so registration code can read the value with oneTBB's public API:

.. code-block:: cpp

   #include <oneapi/tbb/global_control.h>

   auto const n_threads = tbb::global_control::active_value(
     tbb::global_control::max_allowed_parallelism);

Algorithms that use oneTBB internally share the same pool and the same
limit.

Data cells in flight
--------------------

Phlex does not limit how many data cells are being processed at once
(`issue #737 <https://github.com/Framework-R-D/phlex/issues/737>`_).
When a node with a concurrency limit is slower than the nodes that feed it,
incoming messages wait in its queue without bound, and the products they
refer to stay in memory.
A serial output module behind fast upstream algorithms can therefore make
memory use grow with the number of data cells in the job.

Until a limit is available, the options are:

- lower ``-j``;
- register the slow node as ``concurrency::unlimited`` and bound the work
  inside it, for example by having calls wait for one of a fixed pool of
  writer contexts.
  Waiting calls occupy threads, which slows the upstream nodes down.
