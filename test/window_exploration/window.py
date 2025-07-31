from itertools import zip_longest


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
    seq = window_generator(xs)
    return (f(*t) for t in seq)
