#!/usr/bin/env python3
import unittest
import copy
from phlex import AdjustAnnotations

def example_func(a, b=1):
    return a + b

class TestAdjustAnnotations(unittest.TestCase):
    def test_initialization(self):
        ann = {"a": int, "b": int, "return": int}
        wrapper = AdjustAnnotations(example_func, ann, "example_wrapper")
        
        self.assertEqual(wrapper.__name__, "example_wrapper")
        self.assertEqual(wrapper.__annotations__, ann)
        self.assertEqual(wrapper.phlex_callable, example_func)
        # Check introspection attributes are exposed
        self.assertEqual(wrapper.__code__, example_func.__code__)
        self.assertEqual(wrapper.__defaults__, example_func.__defaults__)

    def test_call_by_default_raises(self):
        wrapper = AdjustAnnotations(example_func, {}, "no_call")
        with self.assertRaises(AssertionError) as cm:
            wrapper(1)
        self.assertIn("was called directly", str(cm.exception))

    def test_allow_call(self):
        wrapper = AdjustAnnotations(example_func, {}, "yes_call", allow_call=True)
        self.assertEqual(wrapper(10, 20), 30)

    def test_clone_shallow(self):
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
        self.assertNotEqual(id(wrapper_obj.phlex_callable), id(obj)) # copy was made?
        # copy.copy of a custom object usually creates a new instance if generic

    def test_clone_deep(self):
        class Container:
            def __init__(self): self.data = [1]
            def __call__(self): return self.data[0]
        
        c = Container()
        wrapper = AdjustAnnotations(c, {}, "deep_clone", clone="deep")
        self.assertNotEqual(id(wrapper.phlex_callable), id(c))
        self.assertNotEqual(id(wrapper.phlex_callable.data), id(c.data))

if __name__ == "__main__":
    unittest.main()
