from typing import Optional
from itertools import zip_longest

import window as w


def f(x: int, y: Optional[int]) -> int:
    if y is None:
        return x
    return x + y


def f2(x: int, y: Optional[int], z: Optional[int]) -> int:
    if y is None and z is None:
        return x
    if z is None:
        return x + y
    return x + y + z


def test_window():
    xs = range(1, 6)
    ys = w.window(f, xs=xs)
    assert ys == [3, 5, 7, 9, 5]

    xs = [1]
    ys = w.window(f, xs=xs)
    assert ys == [1]

    xs = []
    ys = w.window(f, xs=xs)
    assert ys == []


def test_window_triplets():
    xs = [1, 2, 3, 4, 5]
    ys = w.window(f2, window_generator=w.make_triplets, xs=xs)
    assert ys == [6, 9, 12, 9, 5]
