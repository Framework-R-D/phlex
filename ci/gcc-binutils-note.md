# GCC `+binutils` vs `~binutils` in Spack

## What `+binutils` does

The Spack `+binutils` variant passes `--with-ld` and `--with-as` to GCC's
`configure`, hard-coding paths to the Spack-managed `binutils` installation.
GCC then preferentially uses that `ld`/`as` at runtime. It does **not** embed
`binutils` into GCC or change what GCC produces.

## The concretization problem

[spack-packages issue #3505][issue-3505] documents a conflict that arises when
GCC is installed `+binutils` and then used as a Spack compiler (new
compiler-as-package model, `%c,cxx=gcc@X`):

- `binutils` has a `type=("build", "link", "run")` dependency on GCC.
- When Spack tracks the run-time dependency chain of the build compiler, it
  pulls in `binutils` and its transitive deps (`zlib-ng`, `zstd`, …).
- Those packages conflict with the same packages required by the environment
  being concretized, producing a concretization failure.

## History of the upstream fix and its revert

| Date | Event |
|------|-------|
| 2026-03-05 | [spack-packages PR #3671][pr-3671] changed binutils dep to `type=("build", "link")`, removing `run` — workaround for #3505 |
| 2026-03-23 | Spack core [PR #52109][pr-52109] merged ("solver: prefer best compiler above one with no penalty on variants") — intended to make the recipe workaround unnecessary |
| 2026-03-24 | [spack-packages PR #3968][pr-3968] **reverted** PR #3671, restoring `type=("build", "link", "run")` for `binutils`, re-introducing the #3505 concretization conflict |

The revert was based on the belief that the Spack core solver fix (PR #52109)
supersedes the recipe workaround. However, PR #52109 addresses compiler
*selection preference*, not the `type=run` dep-chain conflict; the two issues
are distinct.

## Current mitigation

`spack-packages` is pinned to commit `bb6defac18230e7541407d45ddef2f3d4f1202cc`
(2026-03-23, "texlive: new version 20260301"), which postdates the
`type=("build", "link")` fix from PR #3671 but predates the revert in
PR #3968. GCC is therefore installed with the default `+binutils` but with
the safe dep type.

## `~binutils` as a fallback

If the `spack-packages` pin is removed and the upstream issue is still
unresolved, GCC can be installed with `~binutils` as a workaround (Workaround
3 from issue #3505). This means GCC is built without `--with-ld`/`--with-as`
flags and uses whatever `ld`/`as` appear in `$PATH` at runtime — in practice
the system `binutils` from `build-essential`. Functional impact is none in
practice: GCC 15.2 is fully functional and LTO works correctly with modern
system `binutils`. The only difference is that the toolchain is slightly less
hermetic than it would be with a Spack-managed `binutils`.

## When to revisit

Monitor [spack-packages issue #3505][issue-3505]. Once the issue is closed
(either by a confirmed Spack core fix or a stable recipe fix), the
`spack-packages` pin can be removed and the repo updated to `--depth=1 HEAD`.

[issue-3505]: https://github.com/spack/spack-packages/issues/3505
[pr-3671]: https://github.com/spack/spack-packages/pull/3671
[pr-3968]: https://github.com/spack/spack-packages/pull/3968
[pr-52109]: https://github.com/spack/spack/pull/52109
