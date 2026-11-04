Job configuration
=================

A Phlex job is described by a single configuration document, which is
passed to the ``phlex`` executable:

.. code-block:: console

   phlex -c workflow.jsonnet

The document is evaluated with Jsonnet, so plain JSON is also accepted.
This page describes the configuration keys that the framework itself
reads.
Each plugin defines its own additional parameters.

Command-line options
--------------------

``-c``, ``--config <file>``
   The configuration document (required).

``-j``, ``--parallel <n>``
   Maximum number of threads Phlex may use.
   Defaults to oneTBB's default concurrency, normally every hardware thread
   the process may run on.
   On a batch node that does not restrict the job's CPU affinity this is
   the whole machine, so pass ``-j`` matching the cores the job was given.

``--stage <name>``
   Name of the processing stage.

``--version``, ``-h``/``--help``
   Print the version or the option summary.

Options must be spelled out in full; abbreviations are not accepted.
When both are given, ``-j`` overrides the ``max_concurrency`` key and
``--stage`` overrides the ``stage`` key.

Phlex has no options for passing Jsonnet top-level arguments or external
variables.
To parameterize a workflow, evaluate it with the ``jsonnet`` tool first and
hand the result to Phlex.

Library search paths (``jsonnet -J`` or ``JSONNET_PATH``) cannot be set
either, so a configuration passed directly to Phlex must give ``import``
paths relative to the importing file, or as absolute paths.

Top-level keys
--------------

``stage`` (string)
   Name of the processing stage.
   Required unless ``--stage`` is given.
   It may not be empty or ``"CURRENT"``.

``max_concurrency`` (integer)
   Same as ``-j``.

``driver`` (object, required)
   The driver plugin that creates the data cells to process (see
   `Drivers`_ below).

``sources`` (object)
   Plugins registered with ``PHLEX_REGISTER_PROVIDERS`` or
   ``PHLEX_REGISTER_SOURCE``.

``modules`` (object)
   Plugins registered with ``PHLEX_REGISTER_ALGORITHMS``, including output
   modules.

``resources`` (object)
   Plugins registered with ``PHLEX_REGISTER_RESOURCES``.

Only these keys are read.
Any other top-level key is silently ignored, so a misspelled key such as
``module`` has no effect rather than causing an error.

Plugin entries
--------------

``sources``, ``modules`` and ``resources`` are tables whose keys are
*labels* chosen by the user and whose values configure one plugin
instance:

.. code-block:: jsonnet

   {
     stage: 'reconstruction',
     driver: {
       cpp: 'generate_layers',
       layers: { event: { total: 100 } },
     },
     sources: {
       hits: { cpp: 'read_hits', file: 'hits.root' },
     },
     modules: {
       tracking: { cpp: 'track_finder', layer: 'event' },
       py_check: { py: 'checks' },
     },
   }

``cpp: '<name>'``
   Load the C++ plugin library ``lib<name>.so``.

``py: '<module>'``
   Load the Python module ``<module>`` (without the ``.py`` extension)
   through the ``pymodule`` plugin, which must itself be on
   ``PHLEX_PLUGIN_PATH``.
   The module must be importable, for example through ``PYTHONPATH``.
   Supported for ``modules`` and ``sources``, not for ``resources`` or the
   ``driver``.
   Specifying both ``cpp`` and ``py`` is an error.

The whole entry, including ``cpp`` or ``py`` and the plugin's own keys, is
passed to the plugin's registration function as its
``phlex::configuration`` argument.
The framework also adds a ``module_label`` key holding the entry's label.
The label becomes the plugin part of every algorithm name the plugin
registers, so ``product_selector{.creator = "tracking"}`` selects products
created by any algorithm of the ``tracking`` entry above.

Finding plugin libraries
~~~~~~~~~~~~~~~~~~~~~~~~

``PHLEX_PLUGIN_PATH`` must be set to a colon-separated list of directories.
For ``cpp: 'name'``, Phlex loads the first ``libname.so`` it finds on that
path.
If the variable is unset, or no directory contains the library, the job
fails before processing starts.
An empty element in the list (a leading, trailing or doubled colon) makes
Phlex also look in the current directory.

Plugins other than ``pymodule`` are loaded with ``RTLD_LOCAL``, so their
symbols are not available to resolve symbols of libraries loaded later.
A library that a plugin needs only for its side effects (for example a ROOT
dictionary library) should be a direct dependency of that plugin, rather
than relying on another plugin having loaded it first.
Linking with ``--as-needed`` drops such libraries from the plugin's
dependencies because none of their symbols are referenced; linking the
plugin with ``-Wl,--no-as-needed`` keeps them.

Plugins are loaded and registered in this order: all ``resources``, then
all ``modules``, then all ``sources``, then the ``driver``.
Do not rely on the order of plugins within a table.

Drivers
-------

The ``driver`` entry takes a ``cpp`` key, an optional ``uses_sources`` key,
and the driver's own parameters.
Only C++ drivers are supported, and the driver's configuration does not get a
``module_label`` key.

``uses_sources`` (list of names)
   Names of sources whose objects are passed to the driver function, in the
   order listed.
   These are the names given to ``add_source<T>(name, ...)`` in a
   ``PHLEX_REGISTER_SOURCE`` plugin, not the labels of ``sources`` entries;
   a source plugin that registers its source under
   ``config.get<std::string>("module_label")`` makes the two the same.
   Entries that use ``PHLEX_REGISTER_PROVIDERS`` register no source object
   and cannot be listed.
   The driver function's parameters after the cursor (or yielder) must
   match those source types, in the same order.

``generate_layers``
~~~~~~~~~~~~~~~~~~~

Phlex ships one driver, ``generate_layers``, which creates a fixed number of
data cells per layer.
Each key of its ``layers`` table names a layer:

.. code-block:: jsonnet

   {
     driver: {
       cpp: 'generate_layers',
       layers: {
         run: { parent: 'job', total: 2 },
         event: { parent: 'run', total: 100, starting_number: 1 },
       },
     },
   }

``parent`` (default ``'job'``)
   The parent layer.
   The ``job`` layer always exists and is not listed.

``total`` (required)
   Number of data cells created *for each* parent data cell.
   The example above creates 2 runs and 200 events.

``starting_number`` (default ``0``)
   Number of the first data cell under each parent.

Data cells are created in increasing number, but algorithms are not
guaranteed to see them in that order.

Logging
-------

Phlex logs through spdlog.
The ``SPDLOG_LEVEL`` environment variable sets the log level, for example
``SPDLOG_LEVEL=debug``.
At ``debug`` level, Phlex reports how it connected each algorithm's inputs,
which helps when a product selector does not match what you expect.

A plugin may install its own default spdlog logger during registration.
Phlex restores its own default logger once all plugins are loaded, so a
logger installed that way does not stay the default.

Large integers
--------------

Integers above about 2\ :sup:`53` may not survive Jsonnet evaluation
exactly (`issue #243 <https://github.com/Framework-R-D/phlex/issues/243>`_).
Pass large seeds and identifiers as strings.
