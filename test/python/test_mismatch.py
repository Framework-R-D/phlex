"""Test mismatch between input labels and types."""

from typing import Dict, List

from phlex import Variant


def mismatch_func(a: int, b: int) -> int:
    """Add two integers."""
    return a + b


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms."""
    # test a range of possible errors in the annotation and product specifications
    funky1 = List[int] | Dict[int, int]
    funky2 = b'\x80'.decode('gbk', errors='surrogateescape')
    funky3 = "" # empty string

    test_count = 0
    for funky, exc, err in ((funky1, TypeError, "must be unambiguous"),
                            (funky2, UnicodeEncodeError, "surrogates not allowed"),
                            (funky3, TypeError, "unsupported"),):
        funky_input1 = Variant(mismatch_func, {"a": funky, "b": int, "return": int}, "finp1")
        funky_input2 = Variant(mismatch_func, {"a": int, "b": funky, "return": int}, "finp2")
        funky_return = Variant(mismatch_func, {"a": int, "b": int, "return": funky}, "fret")

        for f in (funky_input1, funky_input2, funky_return):
            try:
                m.transform(
                    f,
                    input_family=[{"creator": "input", "layer": "event", "suffix": "i"},
                                  {"creator": "input", "layer": "event", "suffix": "j"}],
                    output_product_suffixes=["sum"],
                )
                assert not "supposed to be here"
            except exc as e:
                test_count += 1
                assert err in str(e)
    assert test_count == 9

    # annotations aren't a dictionary
    funky_func = Variant(mismatch_func, {"a": int, "b": int, "return": int}, "ff")
    funky_func.__annotations__ = [int, int, int]
    try:
        m.transform(
            funky_func,
            input_family=[{"creator": "input", "layer": "event", "suffix": "i"},
                          {"creator": "input", "layer": "event", "suffix": "j"}],
            output_product_suffixes=["sum"],
        )
        assert not "supposed to be here"
    except TypeError as e:
        assert "unknown annotation formatting" in str(e)

    # input_family has 1 element, but function takes 2 arguments
    # This should trigger the error in modulewrap.cpp (this final error is
    # checked by cmake by verify output code and message log)
    m.transform(
        mismatch_func,
        input_family=[{"creator": "input", "layer": "event", "suffix": "a"}],
        output_product_suffixes=["sum"],
    )
