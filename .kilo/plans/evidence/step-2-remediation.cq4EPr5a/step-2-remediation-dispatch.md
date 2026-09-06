# Step 2 targeted remediation dispatch

Executor: `coder-qwen`
Attempt: `2`
Allowed files: `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, `ci/entrypoint.sh`
Prohibited: all Exclusions, commits, image pushes, machine mutation, amd64 cache/binary reuse.

Task: repair only the observed step-2 blockers. Ensure Docker build-time architecture selection is available before every Spack constraint/concretization, maps amd64 to `x86_64_v3` and arm64 to `aarch64`, keeps arm64 LLVM AArch64 and rejects mismatches, establishes GCC for CI and Clang with reproducible GCC 15 `--gcc-toolchain` for developer images, and preserve the GCC 15 ABI/mirror behavior. Do not touch `CMakePresets.json` or unrelated pre-existing paths. Run bounded syntax/static checks and report exact commands and results.

Known evidence: baseline manifest `20260904T151641Z-v3-545de7544a`; prior independent verification `step-2-verify.CkD2F6yi`; prior attempt remains preserved as uncertain.
