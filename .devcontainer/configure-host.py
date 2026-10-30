#!/usr/bin/env python3
"""Persist host choices for Compose; initializeCommand exports cannot reach VS Code."""

from __future__ import annotations

import json
import os
import platform
import subprocess
import sys
import tempfile
from pathlib import Path

# Match the base image default in .devcontainer/Dockerfile.
DEFAULT_IMAGE = "ghcr.io/framework-r-d/phlex-dev:latest"
NATIVE_PLATFORM = "linux/arm64"
INSPECT_PLATFORM = ["podman", "image", "inspect", "--format", "{{.Os}}/{{.Architecture}}"]


def subprocess_cause(exc: subprocess.CalledProcessError) -> str:
    """Keep captured diagnostics, with an exit-status fallback for silent failures."""
    return (exc.stderr or "").strip() or f"exit status {exc.returncode}"


def local_image_platform(image: str) -> str:
    """Return the local os/arch, retaining failed inspection diagnostics for the caller."""
    return subprocess.check_output(
        [*INSPECT_PLATFORM, image], text=True, stderr=subprocess.PIPE
    ).strip()


def ensure_native_image(image: str) -> None:
    """Make a linux/arm64 member of the image available locally without emulation."""
    image_platform: str | None
    inspection_error = None
    try:
        image_platform = local_image_platform(image)
    except subprocess.CalledProcessError as exc:
        image_platform = None
        inspection_error = exc
    if image_platform is None and not image.startswith("localhost/"):
        # A local manifest list is not enough: Compose needs the native member in storage.
        # Pinning the platform prevents the engine from selecting an emulated amd64 member.
        print(f"Pulling {NATIVE_PLATFORM} member of {image} ...")
        try:
            subprocess.check_output(
                ["podman", "pull", "--platform", NATIVE_PLATFORM, image],
                text=True,
                stderr=subprocess.PIPE,
            )
        except subprocess.CalledProcessError as exc:
            raise ValueError(
                f"pull failed for {image}: {subprocess_cause(exc)}; "
                f"run: podman pull --platform {NATIVE_PLATFORM} {image}"
            ) from exc
        try:
            image_platform = local_image_platform(image)
        except subprocess.CalledProcessError as exc:
            inspection_error = exc
    if image_platform is None:
        if image.startswith("localhost/"):
            advice = (
                "build it natively with scripts/generate-image dev and launch VS Code with the "
                "PHLEX_DEV_IMAGE value it reports"
            )
        else:
            advice = f"run: podman pull --platform {NATIVE_PLATFORM} {image}"
        cause = subprocess_cause(inspection_error) if inspection_error else "unresolved reference"
        raise ValueError(
            f"{image} is not available in local Podman storage; "
            f"local inspection failed: {cause}; {advice}"
        )
    if image_platform != NATIVE_PLATFORM:
        raise ValueError(
            f"{image} resolves to {image_platform}, not {NATIVE_PLATFORM}; "
            "select a native image with PHLEX_DEV_IMAGE"
        )


def host_service(system: str, home: Path) -> dict:
    """Use VM paths on macOS, never Unix sockets on the macOS shared filesystem."""
    service: dict = {"volumes": []}
    image = os.environ.get("PHLEX_DEV_IMAGE")
    if system == "Darwin":
        info = subprocess.check_output(
            [
                "podman",
                "info",
                "--format",
                "{{.Host.Arch}} {{.Host.Security.Rootless}} {{.Host.RemoteSocket.Path}}",
            ],
            text=True,
            stderr=subprocess.PIPE,
        ).split()
        if len(info) != 3 or info[:2] != ["arm64", "true"]:
            raise ValueError(
                "macOS development requires a running native arm64 rootless Podman VM"
            )
        socket = info[2].removeprefix("unix://")
        if not socket.startswith("/run/user/") or not socket.endswith("/podman/podman.sock"):
            raise ValueError(f"Unexpected rootless VM socket: {socket}")
        image = image or DEFAULT_IMAGE
        ensure_native_image(image)
        service["platform"] = NATIVE_PLATFORM
        service["network_mode"] = "pasta"
        if os.environ.get("PHLEX_ENABLE_NESTED_PODMAN") == "1":
            # Explicitly opt in to engine control and reduced SELinux isolation.
            service["volumes"].append(f"{socket}:/tmp/podman.sock:ro")
            service["security_opt"] = ["label=disable"]
    else:
        proxy_socket = home / ".podman-proxy/podman.sock"
        if proxy_socket.exists():
            service["volumes"].append(f"{proxy_socket}:/tmp/podman.sock:Z")
    if service["volumes"]:
        service["environment"] = {
            "DOCKER_HOST": "unix:///tmp/podman.sock",
            "CONTAINER_HOST": "unix:///tmp/podman.sock",
        }
    if image:
        service["build"] = {"args": {"PHLEX_DEV_IMAGE": image}}
    auth = home / ".local/share/kilo/auth.json"
    if auth.is_file():
        service["volumes"].append(f"{auth}:/run/phlex-host-auth.json:ro,Z")
    return service


def main() -> None:
    """Atomically replace the generated, gitignored Compose override."""
    target = Path(__file__).with_name("docker-compose.host.yml")
    try:
        service = host_service(platform.system(), Path.home())
    except (ValueError, subprocess.CalledProcessError) as exc:
        # The Dev Containers log shows stderr; a one-line cause beats a traceback.
        message = str(exc)
        if isinstance(exc, subprocess.CalledProcessError):
            message = f"{message}: {subprocess_cause(exc)}"
        print(f"ERROR: {' '.join(message.split())}", file=sys.stderr)
        sys.exit(1)
    # JSON is also YAML, and safely quotes paths containing spaces or punctuation.
    fd, temporary = tempfile.mkstemp(prefix=".compose-host-", dir=target.parent)
    try:
        with os.fdopen(fd, "w") as output:
            output.write(json.dumps({"services": {"phlex-dev": service}}, indent=2) + "\n")
        os.replace(temporary, target)
    finally:
        Path(temporary).unlink(missing_ok=True)


if __name__ == "__main__":
    main()
