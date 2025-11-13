from phlex.registry import register


class Verifier:
    __name__ = 'verifier'

    def __init__(self, sum_total):
        self._sum_total = sum_total

    def __call__(self, isum: int):
        assert isum == self._sum_total


def PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config):
    assert_sum = Verifier(config["sum_total"])
    register(assert_sum, m, config)
