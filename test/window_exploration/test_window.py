from typing import Optional
from dataclasses import dataclass

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
    ys = list(w.window(f, xs=xs))
    assert ys == [3, 5, 7, 9, 5]

    xs = [1]
    ys = list(w.window(f, xs=xs))
    assert ys == [1]

    xs = []
    ys = list(w.window(f, xs=xs))
    assert ys == []


def test_window_triplets():
    xs = [1, 2, 3, 4, 5]
    ys = list(w.window(f2, window_generator=w.make_triplets, xs=xs))
    assert ys == [6, 9, 12, 9, 5]

@dataclass
class Ball:
    """Simple argument type for testing window_generator.

    Matching should be done based on the label attribute.
    """
    value: float
    label: int

def adjacent_matcher(x: int, y: int) -> bool:
    return abs(x - y) == 1

def test_window_generator():
    # Ball come in out-of-order with respect to label.
    xs = [Ball(1.0, 0), Ball(2.0, 3), Ball(3.0, 2), Ball(4.0, 4), Ball(5.0, 1)]

    n_pairs = 0
    for a, b in w.window_generator(xs, adjacent_matcher):
        n_pairs += 1
        if b is None:
            assert a.label == 4
        else:
            assert a.label+1 == b.label
    assert n_pairs == len(xs)
