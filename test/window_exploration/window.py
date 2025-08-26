from collections import defaultdict
from itertools import zip_longest
from typing import Callable, Generator, Protocol, TypeVar

# Type variables for use in protocols and type annotations.
T = TypeVar("T")
S = TypeVar("S")


class WindowGenerator(Protocol[T]):
    """Protocol for a generator that produces windows of elements.

    A type that implements this protocol can be called given a sequence
    (actually a generator) of objects of type T. It returns a sequence
    (again, really a generator) of tuples of objects of type T.

    """

    def __call__(
        self, xs: Generator[T, None, None]
    ) -> Generator[tuple[T, ...], None, None]: ...


# TODO: make_tuples relies on the fact that it is operating
# on a sequence for which it only has to associated adjacent
# elements. We also want do exercise more general functions
# that can be used to create windows that are formed by looking
# at a 'tag' attribute of each element, and which form windows
# by matching elements with matching tags -- where the definition
# of 'match' is left to the a user-specified function.
def make_tuples(xs, size: int):
    """Create tuples of a given size from the list."""
    return zip_longest(*[xs[i:] for i in range(size)])


def make_pairs(xs):
    """Create pairs of elements from the list."""
    return make_tuples(xs, 2)


def make_triplets(xs):
    """Create triplets of elements from the list."""
    return make_tuples(xs, 3)


def window(f, *, window_generator=make_pairs, xs):
    """Apply a function to a window of elements from the list.

    Args:
        f: The function to apply to each window.
        window_generator: A function that creates a window from the list.
        xs: The list of elements to window.
    """
    vals = window_generator(xs)
    return (f(*t) for t in vals)


def window(
    f: Callable[..., S],
    *,
    window_generator: WindowGenerator[T],
    xs: Generator[T, None, None]
) -> Generator[S, None, None]:
    """Apply a function to a window of elements from the list.

    Args:
        f: The function to apply to each window.
        window_generator: A function that creates a window from the list.
        xs: The list of elements to window.
    """
    vals = window_generator(xs)
    return (f(*t) for t in vals)


# TODO: generalize window_generator to allow the user to specify:
#   - the size of the window
#   - the number of leading nulls allowed
#   - the number of trailing nulls allowed
# Question: should these options be passed as argument to window_generator or
# should they be features of the matcher function?
def window_generator(xs, matcher):
    """Generate a sequence of windows from the input.

    This version is very limited. It only creates pairs (not longer tuples).
    It implements a "me and the one after me" window, meaning that the item with
    the "first" label appears only once, and never in the second position in the
    pair.

    Args:
        xs: a generator that yields the input elements.
        matcher: a predicate that takes two labels and determines whether they match.

    Yields:
        A tuple of elements from the input.
        Elements of each tuple will be adjacent, with regards to their labels.
        The second element of the tuple may be None.
    """
    cache = []  # cache of elements
    use_count = defaultdict(int)  # dict of index -> use count
    for x in xs:
        for y in cache:
            if matcher(x.label, y.label):
                if x.label < y.label:
                    yield (x, y)
                else:
                    yield (y, x)
                use_count[x.label] += 1
                use_count[y.label] += 1
                if use_count[y.label] == 2:
                    cache.remove(y)  # relies on __eq__ for the element type
        if use_count[x.label] < 2:
            cache.append(x)

    # No more elements are coming; flush the cache.
    # For each element in the cache, we know that there is not "next" element to
    # be found later, so we yield (element, None).
    for element in cache:
        # We want to yield
        if element.label != 0:
            yield (element, None)
