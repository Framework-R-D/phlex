from phlex.registry import register


def add(i: int, j: int) -> int:
    return i + j


def PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config):
    register(add, m, config)

