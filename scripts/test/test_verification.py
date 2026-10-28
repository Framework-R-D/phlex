"""Regression tests for fail-closed build and CTest verification."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).parent.parent


@pytest.fixture
def tools(tmp_path: Path) -> tuple[Path, dict[str, str]]:
    """Provide recording command doubles without a Spack installation or build.

    The fixture creates a build directory containing spaces, a fake executable
    that logs each invocation to ``COMMAND_LOG``, and symlinks named ``spack``,
    ``cmake``, ``ctest`` and ``nproc`` that point to it.

    Args:
        tmp_path: Pytest-provided temporary directory for the doubles.

    Returns:
        A tuple of the fake build directory path and the environment mapping
        to pass to the verification script.
    """
    build = tmp_path / "build with spaces"
    build.mkdir()
    tool = tmp_path / "tool.py"
    tool.write_text(
        f"#!{sys.executable}\n"
        "import json, os, sys\n"
        "from pathlib import Path\n"
        "name = Path(sys.argv[0]).name\n"
        "args = sys.argv[1:]\n"
        "if name == 'spack':\n"
        "    if args[1] == 'test':\n"
        "        phase = 'list' if '-N' in args else 'test'\n"
        "    else:\n"
        "        phase = {'info': 'discover', 'select': 'select', 'build': 'build'}[args[1]]\n"
        "elif name == 'cmake':\n"
        "    phase = 'build'\n"
        "elif name == 'nproc':\n"
        "    print('2')\n"
        "    sys.exit(0)\n"
        "else:\n"
        "    phase = 'list' if '-N' in args else 'test'\n"
        "with open(os.environ['COMMAND_LOG'], 'a') as log:\n"
        "    log.write(json.dumps([phase, name, args, os.getppid()]) + '\\n')\n"
        "if phase == 'discover':\n"
        "    print(os.environ['TEST_BUILD_DIR'])\n"
        "else:\n"
        "    print(phase + ' stdout')\n"
        "    print(phase + ' stderr', file=sys.stderr)\n"
        "if phase == 'test' and '^misspelled$' in args and '--no-tests=error' in args:\n"
        "    sys.exit(8)\n"
        "sys.exit(int(os.environ.get('FAIL_' + phase.upper(), '0')))\n",
        encoding="utf-8",
    )
    tool.chmod(0o755)
    for name in ("spack", "cmake", "ctest", "nproc"):
        (tmp_path / name).symlink_to(tool)
    env = {
        "PATH": f"{tmp_path}{os.pathsep}{os.environ['PATH']}",
        "COMMAND_LOG": str(tmp_path / "commands.jsonl"),
        "TEST_BUILD_DIR": str(build),
    }
    return build, env


def _commands(env: dict[str, str]) -> list[list]:
    """Read the commands recorded by the fake tools.

    Args:
        env: Environment mapping containing the ``COMMAND_LOG`` path.

    Returns:
        One decoded JSON entry per invocation, each ``[phase, tool, args, ppid]``.
        An empty list is returned if nothing was logged.
    """
    log = Path(env["COMMAND_LOG"])
    return [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []


def _verify(env: dict[str, str], *args: str) -> subprocess.CompletedProcess[str]:
    """Run ``scripts/verify.sh`` with the given arguments.

    Args:
        env: Environment mapping for the child process.
        *args: Command-line arguments forwarded to ``verify.sh``.

    Returns:
        The completed process with captured text stdout and stderr. The return
        code is not checked, so callers assert on it explicitly.
    """
    return subprocess.run(
        ["bash", str(SCRIPTS / "verify.sh"), *args],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


def test_mpd_build_precedes_identical_test_selections(tools: tuple[Path, dict[str, str]]) -> None:
    """Verify the MPD workflow runs discover, select, build, list and test in order.

    All phases must share one parent shell, and each phase's stdout and stderr
    must appear in the verification output.

    Args:
        tools: Build directory and environment from the ``tools`` fixture.

    Raises:
        AssertionError: If the return code, command order, arguments or output
            differ from the expected MPD workflow.
    """
    build, env = tools
    result = _verify(env, "--project", "phlex-work-dir", "--tests", "^identifier$", "--verbose")
    assert result.returncode == 0, result.stderr
    commands = _commands(env)
    assert [c[0] for c in commands] == ["discover", "select", "build", "list", "test"]
    assert commands[0][2] == ["mpd", "info", "--build", "phlex-work-dir"]
    assert commands[1][2] == ["mpd", "select", "--project", "phlex-work-dir"]
    assert commands[2][2] == ["mpd", "build"]
    assert commands[1][3] == commands[2][3] == commands[4][3]
    selection = ["-R", "^identifier$"]
    assert commands[3][1] == commands[4][1] == "spack"
    assert commands[3][2] == ["mpd", "test", "--", *selection, "-N"]
    assert commands[4][2] == [
        "mpd",
        "test",
        "--",
        *selection,
        "-V",
        "--parallel",
        "--output-on-failure",
        "--no-tests=error",
    ]
    for phase in ("build", "list", "test"):
        assert f"{phase} stdout" in result.stdout
        assert f"{phase} stderr" in result.stderr


def test_explicit_build_directory_needs_no_spack(tools: tuple[Path, dict[str, str]]) -> None:
    """Verify a standalone configured build is built and its full suite is run.

    No Spack command may be invoked, and neither CTest call may include ``-R``.

    Args:
        tools: Build directory and environment from the ``tools`` fixture.

    Raises:
        AssertionError: If the return code or the recorded CMake/CTest
            invocations differ from the expected sequence.
    """
    build, env = tools
    result = _verify(env, "--build-dir", str(build))
    assert result.returncode == 0, result.stderr
    commands = _commands(env)
    assert [c[1] for c in commands] == ["cmake", "ctest", "ctest"]
    assert commands[0][2] == ["--build", str(build), "--parallel"]
    assert "-R" not in commands[1][2]
    assert "-R" not in commands[2][2]


@pytest.mark.parametrize("phase", ["discover", "select", "build", "list", "test"])
def test_command_failure_stops_verification(
    tools: tuple[Path, dict[str, str]], phase: str
) -> None:
    """Verify a failing phase stops verification and its status is propagated.

    Args:
        tools: Build directory and environment from the ``tools`` fixture.
        phase: Phase whose fake command is forced to fail with status 23.

    Raises:
        AssertionError: If the return code is not 23 or any phase after the
            failing one was executed.
    """
    _, env = tools
    env[f"FAIL_{phase.upper()}"] = "23"
    result = _verify(env, "--project", "phlex-work-dir")
    assert result.returncode == 23
    phases = ["discover", "select", "build", "list", "test"]
    assert [c[0] for c in _commands(env)] == phases[: phases.index(phase) + 1]


def test_misspelled_selection_fails(tools: tuple[Path, dict[str, str]]) -> None:
    """Verify an empty test selection fails even when discovery succeeds.

    Args:
        tools: Build directory and environment from the ``tools`` fixture.

    Raises:
        AssertionError: If the return code is not 8 or the recorded phases
            differ from build, list, test.
    """
    build, env = tools
    result = _verify(env, "--build-dir", str(build), "--tests", "^misspelled$")
    assert result.returncode == 8
    assert [c[0] for c in _commands(env)] == ["build", "list", "test"]


@pytest.mark.parametrize(
    "args",
    [
        (),
        ("--project",),
        ("--tests",),
        ("--unknown",),
        ("--tests", ""),
        ("--project", "one", "--build-dir", "two"),
    ],
)
def test_invalid_arguments_run_no_commands(
    tools: tuple[Path, dict[str, str]], args: tuple[str, ...]
) -> None:
    """Verify invalid or ambiguous arguments fail before any command runs.

    Args:
        tools: Build directory and environment from the ``tools`` fixture.
        args: Command-line arguments under test.

    Raises:
        AssertionError: If the script exits successfully or any command is
            recorded.
    """
    _, env = tools
    result = _verify(env, *args)
    assert result.returncode != 0
    assert _commands(env) == []


@pytest.mark.parametrize("scenario", ["pass", "empty", "test_failure", "build_failure"])
def test_verification_with_real_cmake_and_ctest(tmp_path: Path, scenario: str) -> None:
    """Verify success and failure paths against a real miniature CMake project.

    The project defines one passing test, one failing test, and an optional
    failing build target. The test is skipped if CMake or CTest is unavailable.

    Args:
        tmp_path: Pytest-provided temporary directory for the project and build.
        scenario: One of ``pass``, ``empty``, ``test_failure`` or
            ``build_failure``, which selects the test pattern and build option.

    Raises:
        AssertionError: If the exit status does not match the scenario, or a
            build failure still reaches the test phase.
    """
    if not shutil.which("cmake") or not shutil.which("ctest"):
        pytest.skip("CMake and CTest are required for the integration test")
    source = tmp_path / "source"
    source.mkdir()
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(verification NONE)\n"
        "enable_testing()\n"
        "add_test(NAME passing COMMAND ${CMAKE_COMMAND} -E true)\n"
        "add_test(NAME failing COMMAND ${CMAKE_COMMAND} -E false)\n"
        "if(FAIL_BUILD)\n"
        "  add_custom_target(broken ALL COMMAND ${CMAKE_COMMAND} -E false)\n"
        "endif()\n",
        encoding="utf-8",
    )
    build = tmp_path / "build with spaces"
    subprocess.run(
        [
            "cmake",
            "-S",
            str(source),
            "-B",
            str(build),
            f"-DFAIL_BUILD={'ON' if scenario == 'build_failure' else 'OFF'}",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    pattern = {"empty": "^misspelled$", "test_failure": "^failing$"}.get(scenario, "^passing$")
    result = _verify(dict(os.environ), "--build-dir", str(build), "--tests", pattern)
    assert (result.returncode == 0) == (scenario == "pass"), result.stdout + result.stderr
    if scenario == "build_failure":
        assert "Test project" not in result.stdout
