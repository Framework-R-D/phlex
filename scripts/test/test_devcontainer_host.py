"""Regression tests for native devcontainer selection and private auth seeding."""

from __future__ import annotations

import json
import os
import runpy
import shutil
import subprocess
from pathlib import Path
from unittest.mock import patch

import pytest

DEVCONTAINER = Path(__file__).resolve().parents[2] / ".devcontainer"
CONFIGURE_HOST = runpy.run_path(str(DEVCONTAINER / "configure-host.py"))
HOST_SERVICE = CONFIGURE_HOST["host_service"]
HOST_MAIN = CONFIGURE_HOST["main"]


@pytest.fixture(autouse=True)
def clean_image_override(monkeypatch):
    """Do not let the developer's selected image influence fixtures."""
    monkeypatch.delenv("PHLEX_DEV_IMAGE", raising=False)
    monkeypatch.delenv("PHLEX_ENABLE_NESTED_PODMAN", raising=False)


def test_darwin_native_defaults(tmp_path):
    """Use pasta and a local arm64 image without engine access or registry pulls."""
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true unix:///run/user/550/podman/podman.sock\n", "linux/arm64\n"],
    ) as command:
        service = HOST_SERVICE("Darwin", tmp_path)
    assert service["network_mode"] == "pasta"
    assert service["platform"] == "linux/arm64"
    assert service["volumes"] == []
    assert "security_opt" not in service
    assert "environment" not in service
    assert service["build"]["args"]["PHLEX_DEV_IMAGE"] == "localhost/phlex-dev:latest"
    assert command.call_args_list[1].args[0] == [
        "podman",
        "image",
        "inspect",
        "--format",
        "{{.Os}}/{{.Architecture}}",
        "localhost/phlex-dev:latest",
    ]


def test_darwin_nested_podman_is_explicit(tmp_path, monkeypatch):
    """Only explicit opt-in grants engine access and disables SELinux labeling."""
    monkeypatch.setenv("PHLEX_ENABLE_NESTED_PODMAN", "1")
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true /run/user/550/podman/podman.sock", "linux/arm64"],
    ):
        service = HOST_SERVICE("Darwin", tmp_path)
    assert service["volumes"] == ["/run/user/550/podman/podman.sock:/tmp/podman.sock:ro"]
    assert service["security_opt"] == ["label=disable"]
    assert service["environment"]["DOCKER_HOST"] == "unix:///tmp/podman.sock"


def test_darwin_explicit_native_image(tmp_path, monkeypatch):
    """An override must be inspected locally before it is selected for macOS."""
    monkeypatch.setenv("PHLEX_DEV_IMAGE", "localhost/phlex-dev:custom")
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true /run/user/550/podman/podman.sock", "linux/arm64"],
    ) as command:
        service = HOST_SERVICE("Darwin", tmp_path)
    assert service["build"]["args"]["PHLEX_DEV_IMAGE"] == "localhost/phlex-dev:custom"
    assert command.call_args.args[0][-1] == "localhost/phlex-dev:custom"


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


@pytest.mark.parametrize("image_platform", ["linux/amd64", "windows/arm64", "", "arm64"])
def test_darwin_rejects_wrong_image_platform(tmp_path, image_platform):
    """Reject both incompatible OS/architecture and malformed inspection results."""
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true /run/user/550/podman/podman.sock", image_platform],
    ):
        with pytest.raises(ValueError, match="does not resolve to a local linux/arm64 image"):
            HOST_SERVICE("Darwin", tmp_path)


@pytest.mark.parametrize(
    "image",
    [
        "ghcr.io/framework-r-d/phlex-dev:latest",
        "ghcr.io/framework-r-d/phlex-dev@sha256:" + "a" * 64,
    ],
)
def test_darwin_multiarch_reference_uses_local_native_member(tmp_path, monkeypatch, image):
    """A registry tag or index digest works when Podman resolves its native member locally."""
    monkeypatch.setenv("PHLEX_DEV_IMAGE", image)
    with patch(
        "subprocess.check_output",
        side_effect=["arm64 true /run/user/550/podman/podman.sock", "linux/arm64"],
    ) as command:
        service = HOST_SERVICE("Darwin", tmp_path)
    assert service["build"]["args"]["PHLEX_DEV_IMAGE"] == image
    assert service["platform"] == "linux/arm64"
    assert command.call_count == 2
    assert command.call_args.args[0][-1] == image


@pytest.mark.parametrize("image", [None, "ghcr.io/framework-r-d/phlex-dev:multiarch"])
def test_missing_image_does_not_fall_back(tmp_path, monkeypatch, image):
    """An absent native image must not cause an automatic registry pull."""
    if image is not None:
        monkeypatch.setenv("PHLEX_DEV_IMAGE", image)
    with patch(
        "subprocess.check_output",
        side_effect=[
            "arm64 true /run/user/550/podman/podman.sock",
            subprocess.CalledProcessError(125, "podman image inspect"),
        ],
    ) as command:
        with pytest.raises(subprocess.CalledProcessError):
            HOST_SERVICE("Darwin", tmp_path)
    assert command.call_count == 2


def test_linux_keeps_base_defaults(tmp_path):
    """Linux requires neither a Podman VM nor an existing credentials file."""
    with patch("subprocess.check_output") as command:
        assert HOST_SERVICE("Linux", tmp_path) == {"volumes": []}
    command.assert_not_called()


def test_linux_mounts_existing_podman_proxy(tmp_path):
    """An available VM proxy socket is mounted and exported for container clients."""
    proxy_socket = tmp_path / ".podman-proxy/podman.sock"
    proxy_socket.parent.mkdir(parents=True)
    proxy_socket.touch()

    service = HOST_SERVICE("Linux", tmp_path)

    assert service["volumes"] == [f"{proxy_socket}:/tmp/podman.sock:Z"]
    assert service["environment"] == {
        "DOCKER_HOST": "unix:///tmp/podman.sock",
        "CONTAINER_HOST": "unix:///tmp/podman.sock",
    }


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


def _patch_host_main(monkeypatch, target, home):
    main_globals = HOST_MAIN.__globals__
    monkeypatch.setitem(main_globals, "__file__", str(target.with_name("configure-host.py")))
    monkeypatch.setattr(main_globals["platform"], "system", lambda: "Linux")
    monkeypatch.setattr(main_globals["Path"], "home", staticmethod(lambda: home))


def test_main_atomically_writes_compose_override(tmp_path, monkeypatch):
    """The generated override replaces stale content and removes its same-dir temp file."""
    target = tmp_path / "checkout with spaces" / "docker-compose.host.yml"
    target.parent.mkdir()
    target.write_text("stale override\n")
    home = tmp_path / "home with spaces"
    auth = home / ".local/share/kilo/auth.json"
    auth.parent.mkdir(parents=True)
    auth.write_text("{}\n")
    _patch_host_main(monkeypatch, target, home)

    with patch.object(CONFIGURE_HOST["os"], "replace", wraps=os.replace) as replace:
        HOST_MAIN()

    replace.assert_called_once()
    temporary = Path(replace.call_args.args[0])
    assert temporary.parent == target.parent
    assert replace.call_args.args[1] == target
    assert not temporary.exists()
    assert target.read_text().endswith("\n")
    service = json.loads(target.read_text())["services"]["phlex-dev"]
    assert service["volumes"] == [f"{auth}:/run/phlex-host-auth.json:ro,Z"]


def test_main_preserves_override_and_cleans_temp_on_replace_failure(tmp_path, monkeypatch):
    """A failed atomic replacement leaves the prior override intact and cleans up."""
    target = tmp_path / "docker-compose.host.yml"
    target.write_text("previous override\n")
    _patch_host_main(monkeypatch, target, tmp_path / "home")

    def fail_replace(source, destination):
        raise OSError("simulated replace failure")

    monkeypatch.setattr(CONFIGURE_HOST["os"], "replace", fail_replace)
    with pytest.raises(OSError, match="simulated replace failure"):
        HOST_MAIN()

    assert target.read_text() == "previous override\n"
    assert list(target.parent.glob(".compose-host-*")) == []


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


@pytest.mark.parametrize(
    "target,llvm,compiler,message",
    [
        ("invalid", "auto", "gcc", "PHLEX_SPACK_TARGET must be"),
        ("aarch64", "x86", "gcc", "Host/target mismatch"),
        ("aarch64", "aarch64", "invalid", "PHLEX_DEFAULT_COMPILER must be"),
    ],
)
@pytest.mark.parametrize("options", ["", "-e", "-u", "-eu"])
def test_entrypoint_validation_preserves_sourcing_shell(
    tmp_path, target, llvm, compiler, message, options
):
    """Invalid configuration fails without closing a shell that sources the entrypoint."""
    entrypoint = (DEVCONTAINER.parent / "ci/entrypoint.sh").read_text()
    validation = entrypoint.split(". /spack/share/spack/setup-env.sh", 1)[0]
    validation = validation.replace(
        "if [ -f /etc/profile.d/phlex-targets.sh ]; then", "if false; then"
    )
    fixture = tmp_path / "entrypoint.sh"
    fixture.write_text(validation.rstrip() + "\n")
    env = {
        **os.environ,
        "PHLEX_SPACK_TARGET": target,
        "PHLEX_LLVM_TARGET": llvm,
        "PHLEX_DEFAULT_COMPILER": compiler,
    }
    command = [
        "bash",
        "-c",
        f'set {options or "+e"}; if source "$1"; then status=0; else status=$?; fi; '
        'printf "survived\\n"; exit "$status"',
        "bash",
        str(fixture),
    ]
    result = subprocess.run(command, env=env, capture_output=True, text=True)
    assert result.returncode == 1
    assert message in result.stderr
    assert "survived" in result.stdout


@pytest.mark.parametrize(
    "mode", ["resolve_existing_numeric_relay_state", "cleanup_owned_numeric_relay"]
)
def test_relay_pid_reuse_drops_record_without_signaling(tmp_path, mode):
    """A live unrelated PID must neither block startup nor be killed during cleanup."""
    definitions = (
        (DEVCONTAINER / "ensure-repos.sh").read_text().split("# --- Main execution ---", 1)[0]
    )
    script = (
        definitions
        + f"""
printf '%s\\n' "$$" > "$(relay_pid_file 25114)"
terminate_pid_gracefully() {{ exit 99; }}
{mode} 25114 25115 || exit 1
[ ! -f "$(relay_pid_file 25114)" ] || exit 2
kill -0 "$$"
"""
    )
    result = subprocess.run(
        ["bash"],
        input=script,
        text=True,
        capture_output=True,
        env={**os.environ, "PHLEX_RELAY_STATE_DIR": str(tmp_path)},
    )
    assert result.returncode == 0, result.stderr
    assert "discarding stale PID file" in result.stderr


@pytest.mark.parametrize("script", ["kilo-env.sh", "ensure-repos.sh"])
def test_production_scripts_work_without_fixture_helpers(tmp_path, script):
    """Copying only production code supports normal startup/cleanup but rejects self-testing."""
    fixture = tmp_path / script
    fixture.write_text((DEVCONTAINER / script).read_text())
    env = {**os.environ, "PHLEX_RELAY_STATE_DIR": str(tmp_path / "relay-state")}
    if script == "kilo-env.sh":
        normal = ["bash", "-eu", "-c", 'source "$1"', "bash", str(fixture)]
        optional = ["bash", "-eu", str(fixture), "--self-test"]
    else:
        normal = ["bash", str(fixture), "--cleanup-owned"]
        optional = ["bash", str(fixture), "--self-test", str(tmp_path / "test-root")]
    result = subprocess.run(normal, env=env, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run(optional, env=env, text=True, capture_output=True)
    assert result.returncode != 0
    assert "self-test" in result.stderr or "self_test" in result.stderr


@pytest.mark.parametrize("signum,status", [("HUP", 129), ("INT", 130), ("TERM", 143)])
def test_relay_self_test_signal_stops_and_cleans_once(tmp_path, signum, status):
    """Signals stop the self-test immediately and do not repeat its EXIT cleanup."""
    definitions = (
        (DEVCONTAINER / "ensure-repos.sh").read_text().split("# --- Main execution ---", 1)[0]
    )
    cleanup = tmp_path / "cleanup"
    script = (
        definitions
        + f"""
source '{DEVCONTAINER}/ensure-repos-self-test.sh'
mode_cleanup_owned() {{ printf 'cleanup\\n' >> '{cleanup}'; }}
start_python_loopback_listener() {{ :; }}
wait_for_source_listener_ready() {{ kill -s {signum} "$$"; }}
mode_self_test_loaded
printf 'unexpected continuation\\n'
"""
    )
    result = subprocess.run(
        ["bash"],
        input=script,
        text=True,
        capture_output=True,
        timeout=10,
        env={**os.environ, "PHLEX_RELAY_STATE_DIR": str(tmp_path / "state")},
    )
    assert result.returncode == status, result.stderr
    assert "unexpected continuation" not in result.stdout
    assert "Self-test: ready named services" not in result.stdout
    assert cleanup.read_text() == "cleanup\n"


@pytest.mark.parametrize(
    "host,target,expected",
    [
        ("x86_64", None, "x86_64_v3"),
        ("amd64", "amd64", "x86_64_v3"),
        ("x86_64", "x86_64_v3", "x86_64_v3"),
        ("arm64", "auto", "aarch64"),
        ("aarch64", "arm64", "aarch64"),
        ("arm64", "aarch64", "aarch64"),
        ("x86_64", "aarch64", None),
        ("arm64", "x86_64_v3", None),
        ("arm64", "invalid", None),
        ("unknown", None, None),
    ],
)
def test_generate_image_native_target_and_tag(tmp_path, host, target, expected):
    """Build tags and Spack targets are normalized and incompatible hosts fail early."""
    script = DEVCONTAINER.parent / "scripts/generate-image"
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    podman = bin_dir / "podman"
    podman.write_text('#!/bin/sh\nprintf "build invocation: %s\\n" "$*"\n')
    podman.chmod(0o700)
    command = f"""
uname() {{ printf '%s\\n' '{host}'; }}
export -f uname
bash "$1" dev
"""
    env = {
        key: value for key, value in os.environ.items() if not key.startswith(("PHLEX_", "GPG_"))
    }
    env.update(HOME=str(tmp_path), PHLEX_TAG_TAG="fixture", PATH=f"{bin_dir}:{os.environ['PATH']}")
    if target is not None:
        env["PHLEX_SPACK_TARGET"] = target
    result = subprocess.run(
        ["bash", "-c", command, "bash", str(script)], env=env, text=True, capture_output=True
    )
    if expected is None:
        assert result.returncode == 1
        assert "build invocation" not in result.stdout
        assert "error:" in result.stderr
    else:
        assert result.returncode == 0, result.stderr
        assert f"--tag phlex-dev:fixture-{expected}" in result.stdout
        assert f"--build-arg PHLEX_SPACK_TARGET={expected}" in result.stdout


@pytest.mark.parametrize("template", ["packages.yaml", "spack.yaml"])
def test_shared_spack_renderer_substitutes_build_inputs(tmp_path, template):
    """Both Spack templates use the same renderer with build-selected versions and targets."""
    dockerfile = (DEVCONTAINER.parent / "ci/Dockerfile").read_text()
    assert dockerfile.count("render_spack_template() {") == 1
    assert dockerfile.count(". /opt/render-spack-template.sh\n") == 2
    renderer = dockerfile.split("<<'RENDER_SPACK_TEMPLATE'\n", 1)[1].split(
        "\nRENDER_SPACK_TEMPLATE", 1
    )[0]
    values = {
        "PHLEX_GCC_VERSION": "16.2",
        "PHLEX_LLVM_TARGET": "aarch64",
        "PHLEX_LLVM_VERSION": "23.1.0",
        "PHLEX_NATIVE_GCC_VERSION": "14.3.0",
        "PHLEX_SPACK_TARGET": "aarch64",
    }
    source = DEVCONTAINER.parent / "ci" / template
    output = tmp_path / "rendered.yaml"
    result = subprocess.run(
        [
            "bash",
            "-c",
            renderer + '\nrender_spack_template "$1" "$2"',
            "bash",
            str(source),
            str(output),
        ],
        env={**os.environ, **values},
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
    expected = source.read_text()
    for key, value in values.items():
        expected = expected.replace("${" + key + "}", value)
    assert output.read_text() == expected
    assert "${PHLEX_" not in output.read_text()


@pytest.mark.parametrize("stage", ["targets", "setup", "activate", "location", "libgcc"])
@pytest.mark.parametrize("options", ["", "-e", "-u", "-eu"])
def test_entrypoint_dependency_failures_return_under_strict_options(tmp_path, stage, options):
    """Dependency failures return predictably under either strict option or their combination."""
    targets = tmp_path / "targets.sh"
    targets.write_text("return 1\n" if stage == "targets" else ":\n")
    setup = tmp_path / "spack-setup.sh"
    setup.write_text(
        "return 1\n"
        if stage == "setup"
        else f"""spack() {{
  if [[ "$1" = env && "$2" = activate ]]; then
    [[ '{stage}' != activate ]]
  else
    [[ '{stage}' != location ]] || return 1
    printf '%s\\n' '{tmp_path}'
  fi
}}
"""
    )
    gcc = tmp_path / "bin/gcc"
    gcc.parent.mkdir()
    gcc.write_text("#!/bin/sh\nexit 1\n")
    gcc.chmod(0o700)
    entrypoint = (DEVCONTAINER.parent / "ci/entrypoint.sh").read_text()
    entrypoint = entrypoint.replace("/etc/profile.d/phlex-targets.sh", str(targets)).replace(
        "/spack/share/spack/setup-env.sh", str(setup)
    )
    fixture = tmp_path / "entrypoint.sh"
    fixture.write_text(entrypoint)
    env = {
        **os.environ,
        "PHLEX_SPACK_TARGET": "aarch64",
        "PHLEX_LLVM_TARGET": "aarch64",
        "PHLEX_DEFAULT_COMPILER": "clang",
    }
    command = [
        "bash",
        "-c",
        f'set {options or "+e"}; if source "$1"; then status=0; else status=$?; fi; '
        'printf "survived\\n"; exit "$status"',
        "bash",
        str(fixture),
    ]
    result = subprocess.run(command, env=env, text=True, capture_output=True)
    assert result.returncode == 1, result.stderr
    assert "return: can only" not in result.stderr
    assert "survived" in result.stdout


@pytest.mark.parametrize(
    "script",
    [
        ".devcontainer/ensure-repos.sh",
        ".devcontainer/setup-repos.sh",
        ".devcontainer/post-create.sh",
        "scripts/generate-image",
    ],
)
def test_execution_only_scripts_reject_sourcing_without_shell_changes(tmp_path, script):
    """Accidental sourcing neither exits the caller nor replaces its options or traps."""
    command = """
trap ':' EXIT HUP INT TERM ERR
before_options=$-
before_traps=$(trap -p)
source "$1" --self-test
status=$?
[[ "$status" = 1 && "$-" = "$before_options" && "$(trap -p)" = "$before_traps" ]] || exit 99
printf 'survived\\n'
"""
    result = subprocess.run(
        ["bash", "-c", command, "bash", str(DEVCONTAINER.parent / script)],
        cwd=tmp_path,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr
    assert result.stdout == "survived\n"
    assert "must be executed, not sourced" in result.stderr


@pytest.mark.parametrize("status", [0, 1])
def test_setup_env_preserves_caller_traps_and_options(tmp_path, status):
    """Sourced setup preserves the caller's traps on both successful and failed setup."""
    script = (DEVCONTAINER.parent / "scripts/setup-env.sh").read_text()
    definitions, main = script.split("# Handle failure explicitly", 1)
    fixture = tmp_path / "setup-env.sh"
    fixture.write_text(
        definitions
        + f"""
detect_build_mode() {{ PHLEX_BUILD_DIR='{tmp_path}'; }}
setup_spack_environment() {{ return 1; }}
activate_spack_environment() {{ return 1; }}
check_build_tools() {{ return {status}; }}
log() {{ :; }}
success() {{ :; }}
"""
        + "# Handle failure explicitly"
        + main
    )
    command = """
set -e
trap ':' EXIT HUP INT TERM ERR
before_options=$-
before_traps=$(trap -p)
if source "$1"; then status=0; else status=$?; fi
[[ "$status" = "$2" && "$-" = "$before_options" && "$(trap -p)" = "$before_traps" ]] || exit 99
printf 'survived\\n'
"""
    env = {key: value for key, value in os.environ.items() if not key.startswith("PHLEX_")}
    result = subprocess.run(
        ["bash", "-c", command, "bash", str(fixture), str(status)],
        env=env,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr
    assert "survived" in result.stdout


def test_setup_env_rejects_execution():
    """The source-only setup script rejects execution before touching the environment."""
    result = subprocess.run(
        ["bash", str(DEVCONTAINER.parent / "scripts/setup-env.sh")],
        text=True,
        capture_output=True,
    )
    assert result.returncode == 1
    assert "should be sourced, not executed" in result.stderr


@pytest.mark.parametrize("sourced", [True, False])
@pytest.mark.parametrize("self_test", [True, False])
def test_kilo_env_exit_paths_preserve_callers(tmp_path, sourced, self_test):
    """The dual-mode Kilo environment script returns when sourced and exits when executed."""
    script = DEVCONTAINER / "kilo-env.sh"
    args = ["--self-test"] if self_test else []
    if sourced:
        command_script = "; ".join(
            (
                'trap ":" EXIT HUP INT TERM ERR',
                "before=$(trap -p)",
                'source "$1" "${@:2}"',
                "status=$?",
                '[[ "$(trap -p)" = "$before" ]] || exit 99',
                'printf "survived\\n"',
                'exit "$status"',
            )
        )
        command = [
            "bash",
            "-c",
            command_script,
            "bash",
            str(script),
            *args,
        ]
    else:
        command = ["bash", str(script), *args]
    env = {key: value for key, value in os.environ.items() if key != "PHLEX_RELAY_FIXTURE_DIR"}
    result = subprocess.run(command, cwd=tmp_path, env=env, text=True, capture_output=True)
    assert result.returncode == int(self_test), result.stderr
    assert ("survived" in result.stdout) == sourced


@pytest.mark.parametrize("options", ["-e", "-u", "-eu"])
def test_entrypoint_is_source_only(options):
    """Executing the environment entrypoint fails before loading Spack or configuring exports."""
    result = subprocess.run(
        ["bash", options, str(DEVCONTAINER.parent / "ci/entrypoint.sh")],
        text=True,
        capture_output=True,
    )
    assert result.returncode == 1
    assert result.stderr.strip() == "ERROR: entrypoint.sh must be sourced, not executed"


@pytest.mark.parametrize("compiler", ["gcc", "clang"])
@pytest.mark.parametrize("options", ["-e", "-u", "-eu"])
def test_entrypoint_success_plain_source_under_strict_options(tmp_path, compiler, options):
    """Plain sourcing succeeds with optional version/target variables absent and options intact."""
    targets = tmp_path / "targets.sh"
    targets.write_text(":\n")
    setup = tmp_path / "spack-setup.sh"
    setup.write_text(f"""spack() {{
  if [[ "$1" = env ]]; then return 0; fi
  printf '%s\\n' '{tmp_path}'
}}
""")
    gcc = tmp_path / "bin/gcc"
    gcc.parent.mkdir()
    gcc.write_text("#!/bin/sh\nprintf '%s\\n' /fixture/lib/gcc/16.2/libgcc.a\n")
    gcc.chmod(0o700)
    fixture = tmp_path / "entrypoint.sh"
    fixture.write_text(
        (DEVCONTAINER.parent / "ci/entrypoint.sh")
        .read_text()
        .replace("/etc/profile.d/phlex-targets.sh", str(targets))
        .replace("/spack/share/spack/setup-env.sh", str(setup))
    )
    env = {key: value for key, value in os.environ.items() if not key.startswith("PHLEX_")}
    env["PHLEX_DEFAULT_COMPILER"] = compiler
    command = f"""
set {options}
trap ':' EXIT ERR HUP INT TERM
before_options=$-
before_traps=$(trap -p)
source "$1"
[[ "$-" = "$before_options" && "$(trap -p)" = "$before_traps" ]]
printf 'ready: %s\\n' "$CXX"
"""
    result = subprocess.run(
        ["bash", "-c", command, "bash", str(fixture)], env=env, text=True, capture_output=True
    )
    assert result.returncode == 0, result.stderr
    assert result.stdout == f"ready: {'g++' if compiler == 'gcc' else 'clang++'}\n"


@pytest.mark.parametrize("options", ["-e", "-u", "-eu"])
@pytest.mark.parametrize("valid_cmake", [True, False])
def test_setup_env_real_source_and_direct_functions_under_strict_options(
    tmp_path, options, valid_cmake
):
    """Exercise real setup and direct helpers without optional environment settings."""
    fixture = tmp_path / "setup-env.sh"
    fixture.write_text((DEVCONTAINER.parent / "scripts/setup-env.sh").read_text())
    home = tmp_path / "home"
    spack_setup = home / "spack/share/spack/setup-env.sh"
    spack_setup.parent.mkdir(parents=True)
    spack_setup.write_text(":\n")
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    for tool in ("cmake", "gcc", "g++", "ninja", "gcov", "lcov", "gcovr"):
        executable = bin_dir / tool
        executable.write_text(
            "#!/bin/sh\n" + ("printf 'cmake version 3.31.0\\n'\n" if valid_cmake else "exit 1\n")
            if tool == "cmake"
            else "#!/bin/sh\nexit 0\n"
        )
        executable.chmod(0o700)
    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("PHLEX_", "SPACK_", "CMAKE_", "CI", "GITHUB_", "BUILD_"))
    }
    env.update(
        HOME=str(home), PATH=f"{bin_dir}:/usr/bin:/bin", PHLEX_BUILD_DIR=str(tmp_path / "build")
    )
    command = f"""
set {options}
before=$-
{'source "$1"' if valid_cmake else 'if source "$1"; then exit 99; fi'}
[[ "$-" = "$before" ]]
log
success
warn
error
if detect_ci_environment; then :; fi
setup_spack_environment
unset BUILD_MODE WORKSPACE_ROOT PHLEX_SPACK_ENV
spack() {{ if [[ "$1" = env && "$2" = status ]]; then return 1; fi; return 0; }}
activate_spack_environment
{"check_build_tools" if valid_cmake else "if check_build_tools; then exit 99; fi"}
printf 'survived\\n'
"""
    result = subprocess.run(
        ["bash", "-c", command, "bash", str(fixture)], env=env, text=True, capture_output=True
    )
    assert result.returncode == 0, result.stderr
    assert "unbound variable" not in result.stderr
    assert "survived" in result.stdout


def test_setup_spack_environment_sources_spack_root_when_command_is_missing(tmp_path):
    """An exported SPACK_ROOT still loads Spack's shell setup when its command is absent."""
    setup_script = tmp_path / "share/spack/setup-env.sh"
    setup_script.parent.mkdir(parents=True)
    setup_script.write_text('spack() { printf "fixture-spack\\n"; }\n')

    setup_env = (DEVCONTAINER.parent / "scripts/setup-env.sh").read_text()
    definitions = setup_env.split("# Main setup flow", 1)[0]
    fixture = tmp_path / "setup-env-functions.sh"
    fixture.write_text(definitions)
    command_script = "\n".join(
        (
            'source "$1"',
            '[[ -n "${SPACK_ROOT:-}" ]]',
            "if command -v spack >/dev/null 2>&1; then exit 99; fi",
            "setup_spack_environment",
            '[[ "$(spack)" = fixture-spack ]]',
        )
    )
    env = {
        key: value for key, value in os.environ.items() if not key.startswith(("PHLEX_", "SPACK_"))
    }
    # Remove the inherited Spack function so restricting PATH makes spack unavailable.
    env.pop("BASH_FUNC_spack%%", None)
    env.update(SPACK_ROOT=str(tmp_path), PATH="/usr/bin:/bin")
    result = subprocess.run(
        ["bash", "-c", command_script, "bash", str(fixture)],
        env=env,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("options", ["-e", "-u", "-eu"])
def test_kilo_env_plain_source_and_direct_functions_under_strict_options(tmp_path, options):
    """Real Kilo production helpers remain usable with strict options and missing arguments."""
    (tmp_path / "relay-map.json").write_text('{"25114":"25115"}\n')
    (tmp_path / "relay-map.env").write_text("PHLEX_HOST_RELAY_25114=25115\n")
    (tmp_path / "kilo.json").write_text('{"url":"http://localhost:25114/v1"}\n')
    command = f"""
set {options}
source "$1"
if kilo_env_run_self_tests; then exit 99; else [[ "$?" = 1 ]]; fi
kilo_env_apply_root "$2"
[[ "$PHLEX_HOST_RELAY_25114" = 25115 && "$KILO_CONFIG_CONTENT" = *host.docker.internal:25115* ]]
kilo_env_find_config_path "$2"
kilo_env_export_path_state "$2"
kilo_env_collect_exports "$2/relay-map.json" "$2/relay-map.env" "$2/kilo.json"
kilo_env_apply_paths "$2/relay-map.json" "$2/relay-map.env" "$2/kilo.json"
kilo_env_sha256 "$2/relay-map.json"
kilo_env_clear_dynamic_exports
kilo_env_clear_failed_exports
if kilo_env_apply_root "$2/incomplete"; then exit 99; else [[ "$?" = 1 ]]; fi
kilo_env_apply_root "$2/absent"
if kilo_env_sha256 "$2/absent"; then exit 99; else [[ "$?" = 1 ]]; fi
kilo_env_seed_stale_exports
kilo_env_assert_export_value PHLEX_HOST_RELAY_25114 stale fixture
kilo_env_assert_file_contains "$2/relay-map.json" 25114 fixture
kilo_env_assert_equal same same fixture
kilo_env_assert_file_empty "$2/absent" fixture
kilo_env_clear_failed_exports
kilo_env_assert_export_absent KILO_CONFIG_CONTENT fixture
hash=$(kilo_env_sha256 "$2/relay-map.json")
kilo_env_assert_hash_unchanged "$2/relay-map.json" "$hash" fixture
[[ "$(kilo_env_extract_output_value 'key=value' key)" = value ]]
if kilo_env_parse_args --invalid; then exit 99; else [[ "$?" = 1 ]]; fi
if kilo_env_run_self_tests; then exit 99; else [[ "$?" = 1 ]]; fi
for function in kilo_env_unset_var_quietly kilo_env_find_config_path kilo_env_export_path_state \
  kilo_env_collect_exports kilo_env_apply_paths kilo_env_apply_root kilo_env_sha256 \
  kilo_env_assert_equal kilo_env_assert_file_contains kilo_env_assert_file_empty \
  kilo_env_assert_export_value kilo_env_assert_export_absent kilo_env_assert_hash_unchanged \
  kilo_env_extract_output_value kilo_env_cleanup_self_test_root kilo_env_run_self_tests_impl; do
  if "$function"; then exit 99; else [[ "$?" = 2 ]]; fi
done
printf 'survived\\n'
"""
    incomplete = tmp_path / "incomplete"
    incomplete.mkdir()
    (incomplete / "relay-map.json").write_text("{}\n")
    env = {key: value for key, value in os.environ.items() if key != "PHLEX_RELAY_FIXTURE_DIR"}
    result = subprocess.run(
        ["bash", "-c", command, "bash", str(DEVCONTAINER / "kilo-env.sh"), str(tmp_path)],
        env=env,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr
    assert "unbound variable" not in result.stderr
    assert "survived" in result.stdout


@pytest.mark.parametrize("sourced", [True, False])
@pytest.mark.parametrize("force_sha256sum_fallback", [False, True])
def test_kilo_env_full_self_test_under_strict_options(tmp_path, sourced, force_sha256sum_fallback):
    """The complete Kilo fixture suite and its cleanup work with errexit/nounset/pipefail."""
    import hashlib

    fixture = tmp_path / "fixture"
    fixture.mkdir()
    (fixture / "relay-map.json").write_text('{"25114":"25115","25300":"25301"}\n')
    (fixture / "relay-map.env").write_text(
        "PHLEX_HOST_RELAY_25114=25115\nPHLEX_HOST_RELAY_25300=25301\n"
    )
    (fixture / "artifact-sha256.txt").write_text(
        "".join(
            f"{hashlib.sha256((fixture / name).read_bytes()).hexdigest()}  {name}\n"
            for name in ("relay-map.json", "relay-map.env")
        )
    )
    scratch = tmp_path / "scratch"
    scratch.mkdir()
    env = {**os.environ, "PHLEX_RELAY_FIXTURE_DIR": str(fixture), "TMPDIR": str(scratch)}
    if force_sha256sum_fallback:
        if shutil.which("sha256sum") is None:
            pytest.skip("sha256sum is unavailable for fallback testing")
        bin_dir = tmp_path / "bin"
        bin_dir.mkdir()
        shasum = bin_dir / "shasum"
        shasum.write_text("#!/bin/sh\nexit 1\n")
        shasum.chmod(0o700)
        env["PATH"] = f"{bin_dir}{os.pathsep}{env['PATH']}"
    command = (
        [
            "bash",
            "-euf",
            "-o",
            "pipefail",
            "-c",
            'source "$1" --self-test',
            "bash",
            str(DEVCONTAINER / "kilo-env.sh"),
        ]
        if sourced
        else ["bash", "-euf", "-o", "pipefail", str(DEVCONTAINER / "kilo-env.sh"), "--self-test"]
    )
    result = subprocess.run(command, env=env, text=True, capture_output=True, timeout=60)
    assert result.returncode == 0, result.stderr + result.stdout
    assert "=== Self-test passed ===" in result.stdout
    assert not list(scratch.iterdir())


@pytest.mark.parametrize("options", ["-e", "-u", "-eu"])
def test_spack_renderer_failure_returns_under_strict_options(tmp_path, options):
    """The sourced build helper returns for missing arguments/settings rather than exiting."""
    dockerfile = (DEVCONTAINER.parent / "ci/Dockerfile").read_text()
    renderer = dockerfile.split("<<'RENDER_SPACK_TEMPLATE'\n", 1)[1].split(
        "\nRENDER_SPACK_TEMPLATE", 1
    )[0]
    command = (
        f"set {options}\n"
        + renderer
        + """
if render_spack_template; then exit 99; else [[ "$?" = 2 ]]; fi
if render_spack_template "$1" "$2"; then exit 99; else [[ "$?" = 1 ]]; fi
printf 'survived\\n'
"""
    )
    source = tmp_path / "template"
    source.write_text("fixture\n")
    env = {key: value for key, value in os.environ.items() if not key.startswith("PHLEX_")}
    result = subprocess.run(
        ["bash", "-c", command, "bash", str(source), str(tmp_path / "output")],
        env=env,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr
    assert "survived" in result.stdout
    assert not (tmp_path / "output").exists()


def test_production_kilo_sourcing_does_not_load_fixture_suite(tmp_path):
    """Normal shell startup defines runtime helpers but does not parse or load the test suite."""
    command_script = "; ".join(
        (
            'source "$1"',
            "declare -F kilo_env_apply_root >/dev/null",
            "if declare -F kilo_env_assert_equal >/dev/null; then exit 99; fi",
            "if declare -F kilo_env_run_self_tests_loaded >/dev/null; then exit 99; fi",
        )
    )
    result = subprocess.run(
        [
            "bash",
            "-euf",
            "-o",
            "pipefail",
            "-c",
            command_script,
            "bash",
            str(DEVCONTAINER / "kilo-env.sh"),
        ],
        cwd=tmp_path,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("helper", ["kilo-env-self-test.sh", "ensure-repos-self-test.sh"])
def test_fixture_helpers_reject_execution(helper):
    """Fixture helper libraries are source-only and do not run tests during import."""
    result = subprocess.run(
        ["bash", "-eu", str(DEVCONTAINER / helper)], text=True, capture_output=True
    )
    assert result.returncode != 0
    assert "sourced" in result.stderr


def test_kilo_test_helper_packaged_outside_profile_directory():
    """Login shells load only production code; explicit self-tests can find their helper."""
    dockerfile = (DEVCONTAINER / "Dockerfile").read_text()
    assert (
        "COPY .devcontainer/kilo-env-self-test.sh /usr/local/lib/phlex/kilo-env-self-test.sh"
        in dockerfile
    )
    assert (
        "!.devcontainer/kilo-env-self-test.sh"
        in (DEVCONTAINER.parent / ".dockerignore").read_text()
    )


def test_relay_production_definitions_do_not_load_fixture_suite(tmp_path):
    """Relay startup and cleanup definitions remain independent of optional fixture code."""
    definitions = (
        (DEVCONTAINER / "ensure-repos.sh").read_text().split("# --- Main execution ---", 1)[0]
    )
    result = subprocess.run(
        ["bash"],
        input=definitions
        + """
declare -F mode_cleanup_owned >/dev/null || exit 99
if declare -F start_python_loopback_listener >/dev/null; then exit 99; fi
if declare -F mode_self_test_loaded >/dev/null; then exit 99; fi
""",
        env={**os.environ, "PHLEX_RELAY_STATE_DIR": str(tmp_path)},
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr
