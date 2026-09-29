"""Regression tests for native devcontainer selection and private auth seeding."""

from __future__ import annotations

import json
import runpy
import subprocess
from pathlib import Path
from unittest.mock import patch

import pytest

DEVCONTAINER = Path(__file__).resolve().parents[2] / ".devcontainer"
HOST_SERVICE = runpy.run_path(str(DEVCONTAINER / "configure-host.py"))["host_service"]


@pytest.fixture(autouse=True)
def clean_image_override(monkeypatch):
    """Do not let the developer's selected image influence fixtures."""
    monkeypatch.delenv("PHLEX_DEV_IMAGE", raising=False)
    monkeypatch.delenv("PHLEX_ENABLE_NESTED_PODMAN", raising=False)


def test_darwin_native_defaults(tmp_path):
    """Use pasta and a local arm64 image without engine access or registry pulls."""
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true unix:///run/user/550/podman/podman.sock\n", "arm64\n"],
    ) as command:
        service = HOST_SERVICE("Darwin", tmp_path)
    assert service["network_mode"] == "pasta"
    assert service["volumes"] == []
    assert "security_opt" not in service
    assert "environment" not in service
    assert service["build"]["args"]["PHLEX_DEV_IMAGE"].startswith("localhost/phlex-dev:")
    assert command.call_args_list[1].args[0][1:3] == ["image", "inspect"]


def test_darwin_nested_podman_is_explicit(tmp_path, monkeypatch):
    """Only explicit opt-in grants engine access and disables SELinux labeling."""
    monkeypatch.setenv("PHLEX_ENABLE_NESTED_PODMAN", "1")
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true /run/user/550/podman/podman.sock", "arm64"],
    ):
        service = HOST_SERVICE("Darwin", tmp_path)
    assert service["volumes"] == ["/run/user/550/podman/podman.sock:/tmp/podman.sock:ro"]
    assert service["security_opt"] == ["label=disable"]
    assert service["environment"]["DOCKER_HOST"] == "unix:///tmp/podman.sock"


@pytest.mark.parametrize(
    "info",
    [
        "amd64 true unix:///run/user/550/podman/podman.sock",
        "arm64 false unix:///run/podman/podman.sock",
        "arm64 true unix:///Users/someone/podman.sock",
        "invalid",
    ],
)
def test_darwin_rejects_wrong_backend(tmp_path, info):
    """Reject emulated, rootful, malformed and host-filesystem socket selections."""
    with patch("subprocess.check_output", return_value=info) as command:
        with pytest.raises(ValueError):
            HOST_SERVICE("Darwin", tmp_path)
    assert command.call_count == 1


def test_darwin_rejects_amd64_image(tmp_path):
    """Never silently run the GHCR amd64 image on an arm64 backend."""
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true /run/user/550/podman/podman.sock", "amd64"],
    ):
        with pytest.raises(ValueError, match="not a local arm64 image"):
            HOST_SERVICE("Darwin", tmp_path)


def test_missing_image_does_not_fall_back(tmp_path):
    """An absent native image must not cause an automatic registry pull."""
    with patch(
        "subprocess.check_output",
        side_effect=[
            "arm64 true /run/user/550/podman/podman.sock",
            subprocess.CalledProcessError(125, "podman image inspect"),
        ],
    ):
        with pytest.raises(subprocess.CalledProcessError):
            HOST_SERVICE("Darwin", tmp_path)


def test_linux_keeps_base_defaults(tmp_path):
    """Linux requires neither a Podman VM nor an existing credentials file."""
    with patch("subprocess.check_output") as command:
        assert HOST_SERVICE("Linux", tmp_path) == {"volumes": []}
    command.assert_not_called()


def test_explicit_image_and_auth_destination(tmp_path, monkeypatch):
    """Explicit images work and host auth never masks the writable private auth file."""
    monkeypatch.setenv("PHLEX_DEV_IMAGE", "localhost/phlex-dev:custom")
    auth = tmp_path / ".local/share/kilo/auth.json"
    auth.parent.mkdir(parents=True)
    auth.write_text("{}\n")
    service = HOST_SERVICE("Linux", tmp_path)
    assert service["build"]["args"]["PHLEX_DEV_IMAGE"] == "localhost/phlex-dev:custom"
    assert service["volumes"] == [f"{auth}:/run/phlex-host-auth.json:ro,Z"]
    assert auth.read_text() == "{}\n"


def test_auth_seed_preserves_other_providers(tmp_path, monkeypatch):
    """Seeding an environment key must preserve existing unrelated provider credentials."""
    auth = tmp_path / "auth.json"
    auth.write_text('{"other-provider": {"type": "api", "key": "fixture"}}\n')
    monkeypatch.setenv("KILO_API_KEY", "fixture-new-key")
    script = (DEVCONTAINER / "post-create.sh").read_text()
    seed = script.split("python3 - <<'PY'\n", 1)[1].split("\nPY", 1)[0]
    with patch("pathlib.Path", return_value=auth):
        exec(compile(seed, "post-create.sh auth seed", "exec"), {})
    result = json.loads(auth.read_text())
    assert result["other-provider"]["key"] == "fixture"
    assert result["fnal-azure"]["key"] == "fixture-new-key"
    assert result["fnal-ow"]["key"] == "fixture-new-key"
    assert auth.stat().st_mode & 0o777 == 0o600


@pytest.mark.parametrize("compiler", ["gcc", "clang"])
@pytest.mark.parametrize("version,native", [("15.3", "13.3.0"), ("16.2", "14.3"), (None, None)])
def test_compiler_uses_build_versions(tmp_path, monkeypatch, compiler, version, native):
    """Both compiler modes use build-provided versions, not fixed GCC major versions."""
    for key, value in [("PHLEX_GCC_VERSION", version), ("PHLEX_NATIVE_GCC_VERSION", native)]:
        if value is None:
            monkeypatch.delenv(key, raising=False)
        else:
            monkeypatch.setenv(key, value)
    gcc = tmp_path / "bin/gcc"
    gcc.parent.mkdir()
    gcc.write_text("#!/bin/sh\nprintf '%s\\n' /fixture/lib/gcc/aarch64/15.3.0/libgcc.a\n")
    gcc.chmod(0o700)
    entrypoint = (DEVCONTAINER.parent / "ci/entrypoint.sh").read_text()
    start = entrypoint.index("gcc_path=$(spack -E location")
    gcc_spec = "gcc" + (f"@{version}" if version else "")
    native_spec = "%c,cxx=gcc" + (f"@{native}" if native else "")
    script = f"""
set -eu
spack() {{
  [[ "$1" = -E && "$2" = location && "$3" = -i ]] || return 1
  [[ "$4" = '{gcc_spec}' && "$5" = '{native_spec}' ]] || return 1
  printf '%s\\n' '{tmp_path}'
}}
PHLEX_DEFAULT_COMPILER={compiler}
unset CXXFLAGS
{entrypoint[start:]}
printf '%s\\n' "$CXX" "${{CXXFLAGS:-}}" "${{PATH%%:*}}"
"""
    result = subprocess.run(["bash", "-c", script], capture_output=True, text=True, check=True)
    lines = result.stdout.splitlines()
    if compiler == "clang":
        assert lines[:2] == ["clang++", "--gcc-install-dir=/fixture/lib/gcc/aarch64/15.3.0"]
    else:
        assert lines == ["g++", "", str(tmp_path / "bin")]


def test_kilo_config_discovery_uses_mount():
    """Production discovery reads the config mount rather than the relay-state mount."""
    script = f"""
source '{DEVCONTAINER}/kilo-env.sh'
[() {{
  if [[ "$1" = -f ]]; then
    [[ "$2" = /root/.config/kilo/kilo.jsonc ]]
  else
    builtin [ "$@"
  fi
}}
kilo_env_find_config_path /run/phlex-host-relays
"""
    result = subprocess.run(["bash", "-c", script], capture_output=True, text=True, check=True)
    assert result.stdout.strip() == "/root/.config/kilo/kilo.jsonc"
