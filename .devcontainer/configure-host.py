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

# Match the base image default in .devcontainer/Dockerfile and the CI workflows.
DEFAULT_IMAGE = "ghcr.io/framework-r-d/phlex-dev:latest"
NATIVE_PLATFORM = "linux/arm64"
INSPECT_PLATFORM = ["podman", "image", "inspect", "--format", "{{.Os}}/{{.Architecture}}"]


def local_image_platform(image: str) -> str | None:
    """Return the os/arch of a locally stored image, or None when it is not stored."""
    try:
        # A missing image is an expected outcome here, so keep Podman's error off stderr.
        return subprocess.check_output(
            [*INSPECT_PLATFORM, image], text=True, stderr=subprocess.DEVNULL
        ).strip()
    except subprocess.CalledProcessError:
        return None


def ensure_native_image(image: str) -> None:
    """Make a linux/arm64 member of the image available locally without emulation."""
    image_platform = local_image_platform(image)
    if image_platform is None and not image.startswith("localhost/"):
        # A local manifest list is not enough: Compose needs the native member in storage.
        # Pinning the platform prevents the engine from selecting an emulated amd64 member.
        print(f"Pulling {NATIVE_PLATFORM} member of {image} ...", file=sys.stderr)
        subprocess.check_output(
            ["podman", "pull", "--platform", NATIVE_PLATFORM, image], text=True
        )
        image_platform = local_image_platform(image)
    if image_platform is None:
        if image.startswith("localhost/"):
            advice = (
                "build it natively with scripts/generate-image dev and launch VS Code with the "
                "PHLEX_DEV_IMAGE value it reports"
            )
        else:
            advice = f"run: podman pull --platform {NATIVE_PLATFORM} {image}"
        raise ValueError(f"{image} is not available in local Podman storage; {advice}")
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
        print(f"ERROR: {exc}", file=sys.stderr)
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
