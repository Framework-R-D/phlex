# Residual work

- Add the read-only `/run/phlex-host-relays` mount through the devcontainer configuration, then validate the mounted production profile path.
- Run native arm64 Spack descriptor expansion/concretization and GCC 15 C++23 compilation.
- Build and inspect the native arm64 CI and development images.
- Validate rootless Podman, pasta networking, socket access, and end-to-end Kilo use on a native host.
- Consider moving the expanded Kilo self-test scaffold out of the profile script when an allowed test path is available.
