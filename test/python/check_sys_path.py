"""An observer to check for output in tests.

Test algorithms produce outputs. To ensure that a test is run correctly,
this observer verifies its result against the expected value.
"""

import sys

class Checker:

    __name__ = 'checker'

    def __init__(self, venv_path: str):
        self._venv_path = venv_path

    def __call__(self, i: int) -> None:
        assert len(sys.path) > 0
        venv_site_packages = f"{sys.prefix}/lib/python{sys.version_info.major}.{sys.version_info.minor}/site-packages"
        assert any(p == venv_site_packages for p in sys.path)


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register check_sys_path for checking sys.path.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.observe(Checker(config["venv"]), input_family = config["input"])
