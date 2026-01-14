"""An observer to check for output in tests.

Test algorithms produce outputs. To ensure that a test is run correctly,
this observer verifies its result against the expected value.
"""

import sys


class Checker:
    """Observer that verifies virtual environment site-packages is in sys.path.

    This checker ensures that when a virtual environment is active, its
    site-packages directory appears in Python's sys.path.
    """

    __name__ = "checker"

    def __init__(self, venv_path: str):
        """Initialize the checker with the expected virtual environment path.

        Args:
            venv_path: Path to the virtual environment directory.
        """
        self._venv_path = venv_path

    def __call__(self, i: int) -> None:
        """Verify that the virtual environment's site-packages is in sys.path.

        Args:
            i: number provided by upstream provider (unused).

        Raises:
            AssertionError: If sys.path is empty or virtual environment
                site-packages is not found in sys.path.
        """
        assert len(sys.path) > 0
        venv_site_packages = f"{sys.prefix}/lib/python{sys.version_info.major}." \
            f"{sys.version_info.minor}/site-packages"
        assert any(p == venv_site_packages for p in sys.path)


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register check_sys_path for checking sys.path.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.observe(Checker(config["venv"]), input_family=config["input"])
