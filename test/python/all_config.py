"""Python-representation of configuration.

This test code comes with a configurtion file that provides exemplars of
all supported types. It then verifies whether this is properly translated
to Python. The actual run is a noop.
"""


class ConfigConsumer:
    """A callable class that "needs" every configuration type.

    Attributes:
        __name__ (str): Identifier for Phlex.
    """

    __name__ = "config_consumer"

    def __init__(self, config):
        """Create a config consumer object.

        Args:
            config (internal): Phlex configuration

        Raises:
            AssertionError: if the provided configuration values do not
                match the expected ones.

        Returns:
            None
        """
        # builtin types
        assert config["a_bool"] == False  # noqa: E712  # we really want to check False
        assert config["an_int"] == -37
        assert config["a_uint"] == 18446744073709551616
        assert config["a_float"] == 3.1415
        assert config["a_string"] == "foo"

        # collection types
        assert config["some_bools"] == (False, True)
        assert config["some_ints"] == (-1, 42, -55)
        assert config["some_uints"] == (18446744073709551616, 29, 137)
        assert config["some_floats"] == (3.1415, 2.71828)
        assert config["some_strings"] == ("aap", "noot", "mies")

    def __call__(self, i: int, j: int) -> None:
        """Dummy routine to do something.

        Consume values `i` and `j` to execute in the Phlex graph.

        Args:
            i (int): The first input value.
            j (int): The second input value.

        Returns:
            None
        """
        pass


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register an instance of `ConfigConsumer` as an observer.

    Use the standard Phlex `observe` registration to insert a node in
    the execution graph that receives two values `i` and `j` for no
    particular reason other than to run inside the execution graph.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    config_consumer = ConfigConsumer(config)
    m.observe(config_consumer, input_family=config["input"])
