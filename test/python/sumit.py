# TODO: consider direct support for numpy.typing, the string representations
# of which are more complicated. Eg. `numpy.typing.NDArray[int]`stringifies
# into: `numpy.ndarray[tuple[int, ...], numpy.dtype[int]]`. This simpler type
# will do for now, since we haven't even decided whether the below will be
# what maps to `std::vector<int>` data products.
def sum_coll(coll: 'ndarray[int]') -> int:
    return sum(coll)

