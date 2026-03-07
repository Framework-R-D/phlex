#!/usr/bin/env python3
"""Unit tests for the phlex.Variant class."""

from pytest import raises

from phlex import MissingAnnotation, Variant


def example_func(a, b=1):
    """Example function for testing."""
    return a + b


ann = {"a": int, "b": int, "return": int}


class TestVariant:
    """Tests for Variant wrapper."""

    def test_initialization(self):
        """Test proper initialization and attribute exposure."""
        wrapper = Variant(example_func, ann, "example_wrapper")

        assert wrapper.__name__ == "example_wrapper"
        assert wrapper.__annotations__ == ann
        assert wrapper.phlex_callable == example_func
        # Check introspection attributes are exposed
        assert wrapper.__code__ == example_func.__code__
        assert wrapper.__defaults__ == example_func.__defaults__

    def test_call_by_default_raises(self):
        """Test that calling the wrapper raises AssertionError by default."""
        wrapper = Variant(example_func, ann, "no_call")
        with raises(AssertionError) as cm:
            wrapper(1)

        assert "was called directly" in str(cm.value)

    def test_allow_call(self):
        """Test that calling is allowed when configured."""
        wrapper = Variant(example_func, ann, "yes_call", allow_call=True)
        assert wrapper(10, 20) == 30

    def test_clone_shallow(self):
        """Test shallow cloning behavior."""
        # For a function, copy.copy just returns the function itself usually,
        # but let's test the flag logic in Variant
        wrapper = Variant(example_func, ann, "clone_shallow", clone=True)
        # function copy is same object
        assert wrapper.phlex_callable == example_func

        # Test valid copy logic with a mutable callable
        class CallableObj:
            def __call__(self):
                pass

        obj = CallableObj()
        wrapper_obj = Variant(obj, {}, "obj_clone", clone=True)
        assert id(wrapper_obj.phlex_callable) != id(obj)  # copy was made?
        # copy.copy of a custom object usually creates a new instance if generic

    def test_clone_deep(self):
        """Test deep cloning behavior."""

        class Container:
            def __init__(self):
                self.data = [1]

            def __call__(self):
                return self.data[0]

        c = Container()
        wrapper = Variant(c, {}, "deep_clone", clone="deep")
        assert id(wrapper.phlex_callable) != id(c)
        assert id(wrapper.phlex_callable.data) != id(c.data)

    def test_missing_annotation_raises(self):
        """Test that MissingAnnotation is raised when a required argument is missing."""

        def func(x, y):
            return x + y

        # Missing 'y'
        incomplete_ann = {"x": int, "return": int}
        with raises(MissingAnnotation) as cm:
            Variant(func, incomplete_ann, "missing_y")

        assert str(cm.value) == "argument 'y' is not annotated"
        assert cm.value.arg == "y"

    def test_missing_optional_annotation_does_not_raise(self):
        """Test that MissingAnnotation is not raised for arguments with default values."""

        def func(x, y=1):
            return x + y

        # Missing 'y', but it has a default value
        incomplete_ann = {"x": int, "return": int}
        wrapper = Variant(func, incomplete_ann, "missing_optional_y")
        assert "x" in wrapper.__annotations__
        assert "y" not in wrapper.__annotations__
