# Deferred native acceptance handoff

## Required environment

Run these checks on a native arm64 host with Spack, the intended rootless Podman machine, pasta networking, and the devcontainer relay mount available. Do not reuse cross-architecture package caches or infer results from this fixture run.

## Required checks

1. Build the CI and dev images with arm64 target normalization and inspect the resulting image architecture.
2. Concretize the rendered native Spack descriptors and compile Phlex with GCC 15 and C++23.
3. Validate rootless Podman socket access and pasta networking.
4. Add and validate the read-only `/run/phlex-host-relays` devcontainer mount, then source the production Kilo profile against its mounted maps and config.
5. Run an end-to-end Kilo provider request through each allowlisted relay.

Until these measurements complete, native and mounted-runtime acceptance remains unavailable.
