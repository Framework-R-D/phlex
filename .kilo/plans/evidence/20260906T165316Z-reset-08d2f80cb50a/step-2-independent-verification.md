# Step 2 independent verification

time=2026-09-06T17:18:58.501434Z
run_id=20260906T165316Z-reset-08d2f80cb50a

## Baseline hashes

CMakePresets.json: baseline=94a9822942ad317daf57f17e45906e8a1f0f7a029e38f708390865ac5eefc53b current=94a9822942ad317daf57f17e45906e8a1f0f7a029e38f708390865ac5eefc53b
ci/Dockerfile: baseline=1b13f67b87ca8b6e4e23ddd7036881c9f8e9e6e34b8c35703ae1cf1dd044f52e current=1b13f67b87ca8b6e4e23ddd7036881c9f8e9e6e34b8c35703ae1cf1dd044f52e
ci/spack.yaml: baseline=a13a04240573269f85e614672b01e2d58ab75f58d6f45b0d66a3a80a0d0cb377 current=a13a04240573269f85e614672b01e2d58ab75f58d6f45b0d66a3a80a0d0cb377
ci/packages.yaml: baseline=5dfe46b4f7cae2552010a99350383725bf4a962e3e49d8d8b28c1be46702f6eb current=5dfe46b4f7cae2552010a99350383725bf4a962e3e49d8d8b28c1be46702f6eb
ci/entrypoint.sh: baseline=3c81ee0faad6018a81d82569b1fb4d06227b757810e77aad742bf9922636e3f3 current=8d3c30e32f61ce04ff76e855d16c3fe7fd0aa86f82741d715852b61ac857c7c2

## Deterministic checks

shell syntax: exit=0
command=bash -n ci/entrypoint.sh
yaml spack: exit=0
command=python3.12 -c import yaml; yaml.safe_load(open("ci/spack.yaml"))
yaml packages: exit=0
command=python3.12 -c import yaml; yaml.safe_load(open("ci/packages.yaml"))
static_architecture_checks=passed
spack_available=no
native_arm64_concretize=not_run (spack unavailable on host)
trivial_cxx23_clang_gcc15_compile=not_run (developer image not built and spack unavailable on host)
caches_and_artifact_reuse=not independently measured

## Result

verification_result=failed
reason=required native concretize and compile measurements are unavailable; worker evidence is not bound to this invocation and is inconsistent with the fresh baseline
