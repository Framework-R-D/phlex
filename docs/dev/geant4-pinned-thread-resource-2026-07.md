# Geant4 and the `dedicated_worker_pool` resource design

*Status: design analysis, July 2026; revised November 2026 for typed synchronous
resource tokens. No real Geant4 backend has been built or tested. The revised
mock and its verification are separate from the backend proposal.*

The [November proposal](geant4-thread-affine-resource-proposal-2026-11.md)
supersedes both the earlier per-run adapter and the subsequent whole-node
handoff design. The current design leaves framework execution unchanged:
ordinary algorithms receive tokens, and typed token methods dispatch only the
resource operation to a dedicated worker.

This note preserves analysis of three inputs:

- The Geant4 multi-threading design page (Book For Toolkit Developers,
  ["Parallelism in Geant4: multi-threading capabilities"](https://geant4.web.cern.ch/documentation/pipelines/master/bftd_html/ForToolkitDeveloper/OOAnalysisDesign/Multithreading/mt.html)).
- CMSSW's `SimG4Core/Application/plugins/OscarMTProducer.cc` (revision
  `b8fdcd5`) and `edm::ThreadHandoff`
  (`FWCore/Utilities/interface/ThreadHandoff.h`). These are earlier source
  observations, not a fresh upstream review.
- The dedicated-worker prototype in
  `test/tbb-preview/resource_limited_threads_test.cpp`. Earlier adapter and
  direct-handle evaluations are historical, not validation of the revised design.

## Short answer

A persistent dedicated thread can supply the OS-thread identity that Geant4's
worker-local state requires. A synchronous token method such as
`token.process_event(p)` can hide that dispatch from an ordinary `g.transform`
algorithm. The method returns detached data, not a Geant4 pointer or reference.
This is a transport design, not proof of a valid Geant4 master/worker lifecycle.

The framework forwards the token normally. Neither the entire algorithm nor the
framework node body moves to the worker. The algorithm submits no execution
lambda. A raw Geant4 pointer is not the desired interface: pointer access cannot
redirect subsequent member calls to another thread.

## What Geant4 requires

From the previously examined MT design page, the binding constraints are:

1. **Event-level parallelism and master/worker control.** Stock
   `G4MTRunManager` creates and controls `G4WorkerRunManager` instances. The
   documented interface says users must never instantiate a worker directly.
2. **Thread-local state.** Split-classes (`G4LogicalVolume`,
   `G4ParticleDefinition`, `G4VUserPhysicsList`, ...), `G4ThreadLocal` statics,
   `G4Cache`, and thread-private singletons depend on the identity of the
   executing OS thread.
3. **Per-thread initialization.** A worker's TLS must be initialized on the
   thread that will process its events, using the configured master's state.
4. **Separate cleanup rules.** The cited documentation describes program-exit
   cleanup for registered objects and thread-private singletons, not universal
   thread-exit cleanup.

Exclusive checkout alone does not satisfy these requirements. Successive TBB
tasks may run on different threads, so all thread-affine Geant4 construction,
event processing, result extraction, and cleanup must stay inside the backend's
worker-thread operations. Detached results may then leave that thread safely.

## CMSSW precedent

In the examined revision, `OscarMTProducer` is a `stream::EDProducer`, with one
instance per concurrent event slot. Its `edm::ThreadHandoff`:

- Owns a dedicated `pthread`, with configurable stack size (10 MiB by default
  in this integration), parked on a condition variable.
- Wraps Geant4 interactions in `runAndWait(...)`, including construction,
  `beginRun`, `produce`, `endRun`, and destruction.
- Blocks the caller until the operation completes and rethrows captured
  exceptions on that caller.

CMSSW also re-establishes services with `ServiceRegistry::Operate` and manages
the CLHEP RNG context with `StaticRandomEngineSetUnset`. These are backend
integration obligations, not effects provided automatically by a thread handoff.
The proposed Phlex resource would hide dispatch and worker-side context setup
behind typed methods rather than expose `runAndWait` to algorithm authors.

## Mapping to the dedicated-worker pool

The pool owns persistent worker threads and their state. A concrete pooled
resource wraps internal worker handles in ordinary copyable, non-owning tokens.
The limiter grants exclusive use of each token while the framework invocation
runs. The resource and pool must outlive all token use; copying a token is not
permission to retain it beyond checkout or use it concurrently.

| Layer | Responsibility |
| --- | --- |
| Framework and limiter | Normal token checkout, forwarding to algorithms, and release |
| Algorithm | Prepare ordinary inputs, call typed token methods, consume detached results |
| Concrete resource token | Synchronously dispatch a typed operation to its associated worker |
| Internal worker transport | On-thread state lifetime, semaphore handoff, completion, exception transport |
| Geant4 backend | Master/worker protocol, worker-side RNG/context, Geant4 operations and cleanup |

An internal `execute(operation)` may privately accept a callable that receives
worker state. It is not an algorithm-facing token API. The transport contract is:

- Semaphores publish the operation and signal completion; the caller blocks
  until the worker is finished using caller-local state.
- For value returns, caller-local `std::optional<Result>` storage is populated
  on the worker and moved out after completion. `void` needs no value storage;
  move-only values are supported. Reference returns are not supported.
- `std::exception_ptr` carries an operation failure back to the caller, which
  rethrows after completion. No `std::future`, `std::promise`,
  `std::packaged_task`, or `std::async` is used.
- Operation captures and returned values may be destroyed on the caller. They
  must not own objects whose destruction requires the Geant4 worker thread.

There is no `thread_affine_token` concept, handle substitution, or special
single-affine-resource limit. Product retrieval, algorithm code surrounding a
token call, and framework output construction stay on the invoking thread.
Only the typed resource operation crosses the boundary. With N checked-out
workers, up to N callers can block while their dedicated threads do work; this
does not establish a global bound on runnable threads or avoid scheduler costs.

## Separate backend proposal: one Geant4 run per job

The token interface does not decide Geant4 run semantics. A separate, unvalidated
backend proposal would begin one job-long Geant4 run during worker setup and
end it during worker teardown. Neither transition would correspond to a Phlex
run boundary. The older `geant4_worker_pool` per-run adapter and broadcast hooks
have been removed; their manual `begin_run`/drain/`end_run` tests did not establish
integration with framework resource checkout.

Under the job-long assumption, run-layer products supply event context and folds
produce per-run summaries. A fold completes for its own inputs and pending
invocations; it is not a global drain or a safe point for shared Geant4 geometry
or physics changes. Per-Phlex-run Geant4 actions or run-dependent geometry would
require another lifecycle design, not merely a fold.

## Backend caveats

1. **One thread per worker lifecycle.** Initialize, process events, and tear down
   each worker on its own persistent OS thread. Eager on-thread factories do
   not authorize bypassing the master's required initialization protocol.
2. **Master ownership and ordering.** The proposed kernel-level route would
   configure the master before workers start and destroy it after all workers,
   on its creating thread. Master and workers belong in one resource object;
   ordering between separately registered resources is not sufficient. Stock
   `G4MTRunManager` owns its threads and is not this proposed route.
3. **Stack size.** Geant4 tracking can need large stacks. `std::jthread` provides
   no portable stack-size control; production support may need a pthread-backed
   worker or another platform-specific mechanism. Mock success says nothing
   about adequate Geant4 stacks.
4. **Failures and partial initialization.** Exception transport is not backend
   recovery. A production resource must define whether a failed worker can be
   reused, handle Geant4 errors that are not C++ exceptions, and provide
   non-throwing cleanup after partial initialization.
5. **Cleanup protocol.** Worker-owned state must be destroyed on its thread
   before joining, respecting Geant4 ownership. Do not assume `G4AutoDelete` or
   singleton cleanup universally runs at thread exit, or delete objects owned by
   Geant4 cleanup machinery. Sequential worker setup/teardown cannot satisfy a
   collective barrier without a different backend protocol.
6. **Context propagation.** Typed methods must arrange worker-side event RNG and
   other required ambient context. Algorithms supply explicit event/run data;
   ordinary framework token forwarding does not propagate TLS or services.

## Validation boundary

The revised mock exposes `token.process_event(unsigned)`, returning
a detached struct containing a value and the worker thread ID, and `token.fail()`,
which throws on the worker. Checks use ordinary-token algorithms through
the real framework and a helper test for the node-body thread boundary.

The configured workspace passed all 195 CTests via `scripts/verify.sh` on
November 16, 2026; the revised resource test also passed 50 consecutive runs.
The graph-level exception test uses a single event: repeated multi-event failure
exposed an unresolved cancellation crash in the installed TBB preview limiter.
See the November note's validation section for the scope and reproduction setup.
Passing mock tests do not validate Geant4 lifecycle, RNG reproducibility, stack
requirements, error recovery, or cleanup.

## Bottom line

Keep the framework's ordinary resource-token model. Put synchronous, typed
dispatch in the resource implementation and keep all thread-affine Geant4 state
behind it. The dedicated-worker mechanism addresses thread identity; the
job-long-run lifecycle and kernel-level Geant4 integration remain separate,
unvalidated backend proposals.
