"""A bunch of observers taking different numbers of arguments.

Simple observers to make sure that taking different numbers of
inputs are thoroughly tested.
"""



def constant_one(i: int) -> int:
    """Constant 1."""
    return 1

def constant_two(i: int) -> int:
    """Constant 2."""
    return 2

def constant_three(i: int) -> int:
    """Constant 3."""
    return 3

def observe_one(i: int) -> None:
    """Observe i; expect 1."""
    assert i == 1

def observe_two(i: int, j: int) -> None:
    """Observe (i, j); expect (1, 2)."""
    assert i == 1
    assert j == 2

def observe_three(i: int, j: int, k: int) -> None:
    """Observe (i, j); expect (1, 2, 3)."""
    assert i == 1
    assert j == 2
    assert k == 3


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register some helpers and observers to test.

    Use the standard Phlex `observe` registration to insert nodes in
    the execution graph that receives values and verifies them.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.transform(constant_one, input_family=[
       {"creator": "input", "layer": "event", "suffix": "i"},
       ],
       output_products=["irrelevant_one"])
    m.transform(constant_two, input_family=[
       {"creator": "input", "layer": "event", "suffix": "i"},
       ],
       output_products=["irrelevant_two"])
    m.transform(constant_three, input_family=[
       {"creator": "input", "layer": "event", "suffix": "i"},
       ],
       output_products=["irrelevant_three"])

    m.observe(observe_one, input_family=[
       {"creator": "constant_one", "layer": "event"},
       ])
    m.observe(observe_two, input_family=[
       {"creator": "constant_one", "layer": "event"},
       {"creator": "constant_two", "layer": "event"},
       ])
    m.observe(observe_three, input_family=[
       {"creator": "constant_one", "layer": "event"},
       {"creator": "constant_two", "layer": "event"},
       {"creator": "constant_three", "layer": "event"},
       ])

