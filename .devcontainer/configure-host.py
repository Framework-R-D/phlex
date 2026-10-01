#!/usr/bin/env python3
"""Persist host choices for Compose; initializeCommand exports cannot reach VS Code."""

from __future__ import annotations

import json
import os
import platform
import subprocess
import tempfile
from pathlib import Path


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
        image = image or "localhost/phlex-dev:2026-09-30"
        architecture = subprocess.check_output(
            ["podman", "image", "inspect", "--format", "{{.Architecture}}", image], text=True
        ).strip()
        if architecture != "arm64":
            raise ValueError(f"{image} is not a local arm64 image; build phlex-dev natively first")
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
    service = host_service(platform.system(), Path.home())
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
