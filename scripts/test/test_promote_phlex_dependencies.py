"""Test dependency-root promotion and rejection of incompatible Spack core APIs."""

from __future__ import annotations

import runpy
import sys
from contextlib import nullcontext
from pathlib import Path
from types import ModuleType, SimpleNamespace
from unittest.mock import Mock

import pytest

PROMOTE_SCRIPT = Path(__file__).resolve().parents[2] / "ci/promote_phlex_dependencies.py"
PROMOTE_MODULE = runpy.run_path(str(PROMOTE_SCRIPT))
PROMOTE = PROMOTE_MODULE["promote_dependencies"]


@pytest.fixture
def environment(monkeypatch):
    """Provide locked dependencies without requiring a local Spack installation."""
    spack = ModuleType("spack")
    spec_module = ModuleType("spack.spec")
    spec_module.__dict__["Spec"] = str
    monkeypatch.setitem(sys.modules, "spack", spack)
    monkeypatch.setitem(sys.modules, "spack.spec", spec_module)

    tbb = SimpleNamespace(name="intel-oneapi-tbb", dag_hash=lambda: "tbb-hash")
    existing = SimpleNamespace(name="cmake", dag_hash=lambda: "cmake-hash")
    phlex = SimpleNamespace(
        name="phlex",
        dag_hash=lambda: "phlex-hash",
        dependencies=Mock(return_value=[tbb, existing]),
    )
    roots = [phlex, existing]
    specs = [phlex, existing, tbb]
    users = set()

    def add(user_spec):
        if user_spec in users:
            return False
        users.add(user_spec)
        return True

    def add_concrete_spec(spec, concrete, *, new=True, group=None):
        roots.append(concrete)

    return SimpleNamespace(
        concrete_roots=lambda: iter(roots),
        all_specs=lambda: iter(specs),
        write_transaction=Mock(side_effect=nullcontext),
        add=Mock(side_effect=add),
        add_concrete_spec=Mock(wraps=add_concrete_spec),
        write=Mock(),
        roots=roots,
        specs=specs,
        users=users,
        phlex=phlex,
        tbb=tbb,
    )


def install_environment_api(monkeypatch, *environments):
    """Provide the Environment constructor imported by the command entry point."""
    environment_module = ModuleType("spack.environment")
    constructor = Mock(side_effect=environments)
    environment_module.__dict__["Environment"] = constructor
    sys.modules["spack"].__dict__["environment"] = environment_module
    monkeypatch.setitem(sys.modules, "spack.environment", environment_module)
    return constructor


def test_promotes_exact_hash_and_skips_existing_roots(environment):
    """Promote only missing roots and preserve every concrete spec."""
    assert PROMOTE(environment) == ["intel-oneapi-tbb"]
    environment.phlex.dependencies.assert_called_once_with(deptype=("link", "run"))
    environment.add.assert_called_once_with("intel-oneapi-tbb/tbb-hash")
    environment.add_concrete_spec.assert_called_once_with(
        spec="intel-oneapi-tbb/tbb-hash", concrete=environment.tbb, new=True
    )
    environment.write.assert_called_once_with(regenerate=False)
    assert len(environment.roots) == 3
    assert len(environment.specs) == 3
    assert PROMOTE(environment) == []
    assert environment.add.call_count == 1


@pytest.mark.parametrize(
    "method",
    [None, lambda spec: None, lambda spec, concrete, *, new, required: None],
)
def test_rejects_incompatible_api_before_mutation(environment, method):
    """Missing methods and changed required arguments produce a clear API error."""
    environment.add_concrete_spec = method
    with pytest.raises(RuntimeError, match=r"Unsupported Spack Environment.add_concrete_spec"):
        PROMOTE(environment)
    environment.write_transaction.assert_not_called()
    environment.add.assert_not_called()
    environment.write.assert_not_called()


def test_rejects_changed_api_behavior(environment):
    """A signature-compatible method must actually register the concrete root."""
    environment.add_concrete_spec = lambda spec, concrete, *, new=True: None
    with pytest.raises(RuntimeError, match="did not register the expected roots"):
        PROMOTE(environment)
    environment.write.assert_not_called()


def test_rejects_changed_concrete_dag(environment):
    """Promotion must not remove or replace any locked dependency."""

    def add_concrete_spec(spec, concrete, *, new=True):
        environment.roots.append(concrete)
        environment.specs.remove(concrete)

    environment.add_concrete_spec = add_concrete_spec
    with pytest.raises(RuntimeError, match="unexpectedly changed the concrete DAG"):
        PROMOTE(environment)
    environment.write.assert_not_called()


@pytest.mark.parametrize("root_count", [0, 2])
def test_rejects_environment_without_exactly_one_phlex_root(environment, root_count):
    """Reject missing or duplicate Phlex roots before changing the environment."""
    environment.roots[:] = [environment.phlex] * root_count

    with pytest.raises(RuntimeError, match="exactly one concrete Phlex root"):
        PROMOTE(environment)

    environment.write_transaction.assert_not_called()
    environment.add.assert_not_called()


def test_rejects_dependency_that_spack_cannot_add_as_a_root(environment):
    """Fail before concrete registration if Spack rejects the exact-hash root."""
    environment.add.side_effect = lambda _: False

    with pytest.raises(RuntimeError, match="already an unconcretized root"):
        PROMOTE(environment)

    environment.add.assert_called_once_with("intel-oneapi-tbb/tbb-hash")
    environment.add_concrete_spec.assert_not_called()
    environment.write.assert_not_called()


def test_main_promotes_and_verifies_persisted_environment(environment, monkeypatch, capsys):
    """Run the command entry point and confirm persisted roots and DAG hashes."""
    env_path = "/tmp/phlex-spack-env"
    monkeypatch.setenv("PHLEX_SPACK_ENV", env_path)
    persisted = SimpleNamespace(
        concrete_roots=lambda: iter(environment.roots),
        all_specs=lambda: iter(environment.specs),
    )
    constructor = install_environment_api(monkeypatch, environment, persisted)

    runpy.run_path(str(PROMOTE_SCRIPT), run_name="__main__")

    assert [call.args for call in constructor.call_args_list] == [(env_path,), (env_path,)]
    assert "Promoted Phlex dependencies: intel-oneapi-tbb" in capsys.readouterr().out


@pytest.mark.parametrize(
    ("persisted_roots", "persisted_specs", "error_message"),
    [
        (False, True, "did not persist the promoted dependency roots"),
        (True, False, "changed the concrete DAG while writing the environment"),
    ],
)
def test_main_rejects_inconsistent_persisted_environment(
    environment, monkeypatch, persisted_roots, persisted_specs, error_message
):
    """Reject a persisted environment that loses roots or changes its DAG."""
    monkeypatch.setenv("PHLEX_SPACK_ENV", "/tmp/phlex-spack-env")
    roots = environment.roots if persisted_roots else environment.roots[:-1]
    specs = environment.specs if persisted_specs else environment.specs[:-1]
    persisted = SimpleNamespace(
        concrete_roots=lambda: iter(roots),
        all_specs=lambda: iter(specs),
    )
    install_environment_api(monkeypatch, environment, persisted)

    with pytest.raises(SystemExit, match=error_message):
        PROMOTE_MODULE["main"]()
