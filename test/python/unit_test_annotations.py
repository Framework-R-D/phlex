#!/usr/bin/env python3
"""Unit tests for the phlex.AdjustAnnotations class."""

import unittest

from phlex import AdjustAnnotations


def example_func(a, b=1):
    """Example function for testing."""
    return a + b


class TestAdjustAnnotations(unittest.TestCase):
    """Tests for AdjustAnnotations wrapper."""

    def test_initialization(self):
        """Test proper initialization and attribute exposure."""
        ann = {"a": int, "b": int, "return": int}
        wrapper = AdjustAnnotations(example_func, ann, "example_wrapper")

        self.assertEqual(wrapper.__name__, "example_wrapper")
        self.assertEqual(wrapper.__annotations__, ann)
        self.assertEqual(wrapper.phlex_callable, example_func)
        # Check introspection attributes are exposed
        self.assertEqual(wrapper.__code__, example_func.__code__)
        self.assertEqual(wrapper.__defaults__, example_func.__defaults__)

    def test_call_by_default_raises(self):
        """Test that calling the wrapper raises AssertionError by default."""
        wrapper = AdjustAnnotations(example_func, {}, "no_call")
        with self.assertRaises(AssertionError) as cm:
            wrapper(1)
        self.assertIn("was called directly", str(cm.exception))

    def test_allow_call(self):
        """Test that calling is allowed when configured."""
        wrapper = AdjustAnnotations(example_func, {}, "yes_call", allow_call=True)
        self.assertEqual(wrapper(10, 20), 30)

    def test_clone_shallow(self):
        """Test shallow cloning behavior."""
        # For a function, copy.copy just returns the function itself usually,
        # but let's test the flag logic in AdjustAnnotations
        wrapper = AdjustAnnotations(example_func, {}, "clone_shallow", clone=True)
        # function copy is same object
        self.assertEqual(wrapper.phlex_callable, example_func)

        # Test valid copy logic with a mutable callable
        class CallableObj:
            def __call__(self): pass

        obj = CallableObj()
        wrapper_obj = AdjustAnnotations(obj, {}, "obj_clone", clone=True)
        self.assertNotEqual(id(wrapper_obj.phlex_callable), id(obj))  # copy was made?
        # copy.copy of a custom object usually creates a new instance if generic

    def test_clone_deep(self):
        """Test deep cloning behavior."""
        class Container:
            def __init__(self): self.data = [1]
            def __call__(self): return self.data[0]

        c = Container()
        wrapper = AdjustAnnotations(c, {}, "deep_clone", clone="deep")
        self.assertNotEqual(id(wrapper.phlex_callable), id(c))
        self.assertNotEqual(id(wrapper.phlex_callable.data), id(c.data))


if __name__ == "__main__":
    unittest.main()
