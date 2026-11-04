Resources
=========

A *resource* is an object that the framework owns for the whole job and
hands to the algorithms that declare they use it, such as a calibration
service, a geometry, or a library that may only be used by one caller at a
time.
Resources were introduced in Phlex v0.4.0.

Kinds of resources
------------------

Unlimited resources
   Any number of calls may use the resource at the same time.
   The type must *not* define a ``token_type`` member.
   Algorithms receive a ``T const*``.

Serialized resources
   Across *all* algorithms that declare the resource, at most one call
   holding it runs at a time.
   The type must define a ``token_type`` member, and algorithms receive a
   value of that type:

   - a pointer to the resource, if ``T*`` converts to ``token_type``
     (for example ``using token_type = T const*;``);
   - a copy of the resource, if ``token_type`` is ``T`` itself, in which
     case ``T`` must be copy-constructible;
   - otherwise, the result of ``T::token()``, which must return something
     convertible to ``token_type`` and be either a ``static`` member
     function or a ``const``-qualified member function.
     A ``token()`` that is neither is rejected by ``add_serialized_resource``.

   This is the framework's way to protect a non-thread-safe library that
   several algorithms share.
   A serialized resource limits how many calls run at once; it does not make
   the calls run on one particular thread.

Each resource type may be registered only once per job.

Registering resources
---------------------

Resources are registered by a plugin that uses ``PHLEX_REGISTER_RESOURCES``
from ``phlex/resource.hpp`` and links to the ``phlex::resource`` CMake
target:

.. code-block:: cpp

   #include "phlex/configuration.hpp"
   #include "phlex/resource.hpp"

   struct calibration {
     explicit calibration(int version) : version{version} {}
     int version;
   };

   struct legacy_generator {
     using token_type = legacy_generator*;
     // ... wraps a library that is not thread-safe
   };

   PHLEX_REGISTER_RESOURCES(r, config)
   {
     r.add_unlimited_resource<calibration>(config.get<int>("version"));
     r.add_serialized_resource<legacy_generator>();
   }

Define the resource types in a header that is included both by the plugin
that registers them and by every algorithm module that uses them, as
``test/plugins/resources_for_testing.hpp`` does.
Header-only types like these need no separate shared library.

The arguments of ``add_unlimited_resource`` and ``add_serialized_resource``
are forwarded to the resource's constructor.

Resource plugins are configured in the top-level ``resources`` table and
are loaded before all other plugins.
Only C++ resource plugins are supported:

.. code-block:: jsonnet

   {
     resources: {
       services: { cpp: 'my_resources', version: 3 },
     },
   }

Using resources
---------------

An algorithm declares the resources it uses by passing ``resource<T>{}``
(from ``phlex/core/resource_api.hpp``) to ``input_family(...)`` after all
product selectors.
The algorithm receives one extra parameter per resource, after its product
parameters and in the same order:

.. code-block:: cpp

   m.transform(
      "generate",
      [](int seed, calibration const* calib, legacy_generator* gen) -> double { /* ... */ },
      concurrency::unlimited)
     .input_family(product_selector{.creator = "seeds", .layer = "event"},
                   resource<calibration>{},
                   resource<legacy_generator>{});

A node can run no more calls at once than both its own ``concurrency``
argument and its serialized resources allow.
Using a resource type that no plugin registered is an error.

Limitations
-----------

- Providers cannot use resources
  (`issue #945 <https://github.com/Framework-R-D/phlex/issues/945>`_).
- Output modules registered with ``output(...)`` cannot use resources,
  because they have no ``input_family(...)``.
- Algorithms written in Python cannot use resources.
- For an unfold, the resource parameters go to the unfold function, not to
  the unfolder's constructor
  (`issue #946 <https://github.com/Framework-R-D/phlex/issues/946>`_).
- Resources with a pool of several tokens are not supported yet.
- Phlex needs `oneTBB 2023.0.0 or later
  <https://github.com/uxlfoundation/oneTBB/releases#release-v2023.0.0>`_,
  which provides the preview ``flow::resource_limiter`` class; CMake
  configuration fails without it.
  The ``phlex::core`` target defines ``TBB_PREVIEW_FLOW_GRAPH_RESOURCE_LIMITING``
  for its users.
