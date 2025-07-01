"""
Minimal access to Phlex from cppyy.

cppyy needs access to core Phlex classes to complete the registration, which
is done through header parsing. Only the relevant header files are included
and no libraries are loaded as this module is only for use in the `phlex`
executable, which is already appropriately linked.
"""

import cppyy
import os

# locate phlex through the environment
PHLEX_INSTALL = os.environ["PHLEX_INSTALL"]

# load phlex headers
cppyy.add_include_path(os.path.join(PHLEX_INSTALL, "include"))
cppyy.include("phlex/core/framework_graph.hpp")
cppyy.include("phlex/configuration.hpp")

# open phlex namespace for convenience in JITed C++ snippets
cppyy.cppdef("using namespace phlex::experimental;")

# `with` is a keyword in Python, so can not be the name of a method; fix this
# by renaming it to `with_` for all phlex classes
def fixwith(klass, name):
    try:
        klass.with_ = getattr(klass, 'with')
    except AttributeError:
        pass  # nothing to fix

cppyy.py.add_pythonization(fixwith, "phlex::experimental")

