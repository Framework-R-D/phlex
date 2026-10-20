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
    """Provide recording command doubles without a Spack installation or build."""
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
        "    phase = {'info': 'discover', 'select': 'select', 'build': 'build'}[args[1]]\n"
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
    log = Path(env["COMMAND_LOG"])
    return [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []


def _verify(env: dict[str, str], *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["bash", str(SCRIPTS / "verify.sh"), *args],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


def test_mpd_build_precedes_identical_test_selections(tools: tuple[Path, dict[str, str]]) -> None:
    """Discover, select, build, list and run in one shell without losing output."""
    build, env = tools
    result = _verify(env, "--project", "phlex-work-dir", "--tests", "^identifier$", "--verbose")
    assert result.returncode == 0, result.stderr
    commands = _commands(env)
    assert [c[0] for c in commands] == ["discover", "select", "build", "list", "test"]
    assert commands[0][2] == ["mpd", "info", "--build", "phlex-work-dir"]
    assert commands[1][2] == ["mpd", "select", "--project", "phlex-work-dir"]
    assert commands[2][2] == ["mpd", "build"]
    assert commands[1][3] == commands[2][3] == commands[4][3]
    selection = ["--test-dir", str(build), "-R", "^identifier$"]
    assert commands[3][2] == [*selection, "-N"]
    assert commands[4][2] == [
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
    """A standalone configured build is built and its entire suite is run."""
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
    """Propagate each failing command's status and skip all subsequent commands."""
    _, env = tools
    env[f"FAIL_{phase.upper()}"] = "23"
    result = _verify(env, "--project", "phlex-work-dir")
    assert result.returncode == 23
    phases = ["discover", "select", "build", "list", "test"]
    assert [c[0] for c in _commands(env)] == phases[: phases.index(phase) + 1]


def test_misspelled_selection_fails(tools: tuple[Path, dict[str, str]]) -> None:
    """CTest execution rejects a selection even when empty discovery succeeds."""
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
    """Invalid or ambiguous input fails before discovery, building or testing."""
    _, env = tools
    result = _verify(env, *args)
    assert result.returncode != 0
    assert _commands(env) == []


@pytest.mark.parametrize("scenario", ["pass", "empty", "test_failure", "build_failure"])
def test_verification_with_real_cmake_and_ctest(tmp_path: Path, scenario: str) -> None:
    """Validate success and failure paths using an actual miniature CMake project."""
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
