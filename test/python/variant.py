"""Annotation helper for C++ typing variants.

Python algorithms are generic, like C++ templates, but the Phlex registration
process requires a single unique signature. These helpers generate annotated
functions for registration with the proper C++ types.
"""

import collections
import copy
from typing import Any, Callable


class MissingAnnotation(Exception):
    def __init__(self, arg):
        self.arg = arg

    def __str__(self):
        return "argument '%s' is not annotated" % self.arg


class Variant:
    """Wrapper to associate custom annotations with a callable.

    This class wraps a callable and provides custom ``__annotations__`` and
    ``__name__`` attributes, allowing the same underlying function or callable
    object to be registered multiple times with different type annotations.

    By default, the provided callable is kept by reference, but can be cloned
    (e.g. for callable instances) if requested.

    Phlex will recognize the "phlex_callable" data member, allowing an unwrap
    and thus saving an indirection. To detect performance degradation, the
    wrapper is not callable by default.

    Attributes:
        phlex_callable (Callable): The underlying callable (public).
        __annotations__ (dict): Type information of arguments and return product.
        __name__ (str): The name associated with this variant.

    Examples:
        >>> def add(i: Number, j: Number) -> Number:
        ...     return i + j
        ...
        >>> int_adder = variant(add, {"i": int, "j": int, "return": int}, "iadd")
    """
    def __init__(
        self,
        f: Callable,
        annotations: dict[str, str | type | Any],
        name: str,
        clone: bool | str = False,
        allow_call: bool = False,
    ):
        """Annotate the callable F.

        Args:
            f (Callable): Annotable function.
            annotations (dict): Type information of arguments and return product.
            name (str): Name to assign to this variant.
            clone (bool|str): If True (or "deep"), creates a shallow (deep) copy
                of the callable.
            allow_call (bool): Allow this wrapper to forward to the callable.
        """
        if clone == 'deep':
            self.phlex_callable = copy.deepcopy(f)
        elif clone:
            self.phlex_callable = copy.copy(f)
        else:
            self.phlex_callable = f

        # annotions are expected as an ordinary dict and should be ordered, but
        # we do not require it, so re-order based on the function's co_varnames
        self.__annotations__ = collections.OrderedDict()
        try:
            args = self.phlex_callable.__code__.co_varnames
        except AttributeError:
            # callable instance; get the dispatcher method and offset `self`
            args = self.phlex_callable.__call__.__code__.co_varnames[1:]

        try:
            for v in args:
                self.__annotations__[v] = annotations[v]
        except KeyError as e:
            raise MissingAnnotation(v)

        self.__annotations__['return'] = annotations.get('return', None)

        self.__name__ = name
        self._allow_call = allow_call

    def __call__(self, *args, **kwargs):
        """Raises an error if called directly.

        Variant instances should not be called directly. The framework should
        extract ``phlex_callable`` instead and call that.

        Raises:
            AssertionError: To indicate incorrect usage, unless overridden.
        """
        assert self._allow_call, (
            f"TypedVariant '{self.__name__}' was called directly. "
            f"The framework should extract phlex_callable instead."
        )
        return self.phlex_callable(*args, **kwargs)  # type: ignore

