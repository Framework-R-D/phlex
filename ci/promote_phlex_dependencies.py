#!/usr/bin/env spack python
"""Promote locked Phlex runtime dependencies to roots without re-concretizing."""

from __future__ import annotations

import inspect
import os
import sys


def promote_dependencies(env) -> list[str]:
    """Register existing link/run dependencies as abstract and concrete roots."""
    from spack.spec import Spec

    phlex_roots = [spec for spec in env.concrete_roots() if spec.name == "phlex"]
    if len(phlex_roots) != 1:
        raise RuntimeError("Expected exactly one concrete Phlex root in the environment")
    phlex = phlex_roots[0]

    # This core API is not covered by Spack's versioned Package API contract.
    method = getattr(env, "add_concrete_spec", None)
    try:
        if not callable(method):
            raise TypeError("add_concrete_spec is missing or not callable")
        signature = inspect.signature(method)
        signature.bind(spec=phlex, concrete=phlex, new=True)
    except (TypeError, ValueError) as exc:
        raise RuntimeError(
            "Unsupported Spack Environment.add_concrete_spec() API: expected a callable "
            "accepting spec, concrete, and new=True with no other required arguments. "
            "Review the promotion script against this Spack revision before rebuilding."
        ) from exc

    hashes = {spec.dag_hash() for spec in env.all_specs()}
    roots = {spec.dag_hash() for spec in env.concrete_roots()}
    dependencies = phlex.dependencies(deptype=("link", "run"))
    promoted = []

    with env.write_transaction():
        for dep in dependencies:
            dep_hash = dep.dag_hash()
            if dep_hash in roots:
                continue
            user_spec = Spec(f"{dep.name}/{dep_hash}")
            if not env.add(user_spec):
                raise RuntimeError(f"Dependency {user_spec} is already an unconcretized root")
            env.add_concrete_spec(spec=user_spec, concrete=dep, new=True)
            roots.add(dep_hash)
            promoted.append(dep.name)

        if {spec.dag_hash() for spec in env.concrete_roots()} != roots:
            raise RuntimeError("Spack add_concrete_spec() did not register the expected roots")
        if {spec.dag_hash() for spec in env.all_specs()} != hashes:
            raise RuntimeError("Dependency promotion unexpectedly changed the concrete DAG")

        # Installation layers regenerate the view after dependencies are installed.
        env.write(regenerate=False)

    return promoted


def main() -> None:
    """Update the image's locked environment and verify the persisted roots."""
    try:
        import spack.environment as ev

        env_path = os.environ["PHLEX_SPACK_ENV"]
        env = ev.Environment(env_path)
        promoted = promote_dependencies(env)
        expected_roots = {spec.dag_hash() for spec in env.concrete_roots()}
        expected_hashes = {spec.dag_hash() for spec in env.all_specs()}
        persisted = ev.Environment(env_path)
        if {spec.dag_hash() for spec in persisted.concrete_roots()} != expected_roots:
            raise RuntimeError("Spack did not persist the promoted dependency roots")
        if {spec.dag_hash() for spec in persisted.all_specs()} != expected_hashes:
            raise RuntimeError("Spack changed the concrete DAG while writing the environment")
    except Exception as exc:
        sys.exit(f"ERROR: Cannot promote Phlex dependency roots: {exc}")

    print("Promoted Phlex dependencies: " + (", ".join(promoted) or "none (already roots)"))


if __name__ == "__main__":
    main()
