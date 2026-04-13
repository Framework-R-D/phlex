"""Test mismatch between input labels and types."""

from phlex import Variant


def mismatch_func(a: int, b: int) -> int:
    """Add two integers."""
    return a + b


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms."""
    # test a range of possible errors in the annotation and product specifications
    funky = b'\x80'.decode('gbk', errors='surrogateescape')

    funky_input1 = Variant(mismatch_func, {"a": funky, "b": int, "return": int}, "funky_input1")
    funky_input2 = Variant(mismatch_func, {"a": int, "b": funky, "return": int}, "funky_input2")
    funky_return = Variant(mismatch_func, {"a": int, "b": int, "return": funky}, "funky_return")

    test_count = 0
    for f in (funky_input1, funky_input2, funky_return):
        try:
            m.transform(
                f,
                input_family=[{"creator": "input", "layer": "event", "suffix": "i"},
                              {"creator": "input", "layer": "event", "suffix": "j"}],
                output_product_suffixes=["sum"],
            )
            assert not "supposed to be here"
        except UnicodeEncodeError as e:
            test_count += 1
            assert 'surrogates not allowed' in str(e) # consequence of C++ assuming UTF-8
    assert test_count == 3

    # input_family has 1 element, but function takes 2 arguments
    # This should trigger the error in modulewrap.cpp
    m.transform(
        mismatch_func,
        input_family=[{"creator": "input", "layer": "event", "suffix": "a"}],
        output_product_suffixes=["sum"],
    )
