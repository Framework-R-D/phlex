# Geant4 and the `pinned_thread_resource` design

*Status: design analysis, July 2026. Investigates whether the pinned-thread
resource model prototyped in `test/tbb-preview/resource_limiting_test.cpp` can
accommodate Geant4's multi-threading constraints.*

This note analyzes three inputs:

- The Geant4 multi-threading design page (Book For Toolkit Developers,
  "Parallelism in Geant4: multi-threading capabilities").
- CMSSW's `SimG4Core/Application/plugins/OscarMTProducer.cc` (revision
  `b8fdcd5`), which adapts an event-processing framework to Geant4's threading
  model, together with its `edm::ThreadHandoff` helper
  (`FWCore/Utilities/interface/ThreadHandoff.h`).
- The `managed_thread` + `flow::resource_limiter` prototype in
  `test/tbb-preview/resource_limiting_test.cpp` (the "std::threads as limited
  resources" test case).

The question: can the `pinned_thread_resource` approach — in which algorithm
writers do *not* explicitly manage thread state and merely use a pinned-thread
resource token — work given Geant4's constraints?

## Short answer

Yes. The `pinned_thread_resource` approach is not only possible under Geant4's
constraints, it is a cleaner realization of the same mechanism CMSSW implements
by hand. The `managed_thread` in the prototype is functionally the same object
as CMSSW's `edm::ThreadHandoff`. Geant4's binding requirement — that all Geant4
calls for a given worker happen on one specific OS thread — maps exactly onto
what a pinned-thread resource token provides. The design goal (algorithm
writers do not manage thread state) is achievable, with one important caveat
about *what* gets pinned.

## What Geant4 actually requires

From the MT design page, the binding constraints are:

1. **Event-level parallelism, master/worker model.** One `G4MTRunManager`
   (master) creates and controls N `G4WorkerRunManager` instances. Users must
   never instantiate a worker directly.
2. **Thread-local storage is the thread-safety mechanism.** Split-classes
   (`G4LogicalVolume`, `G4ParticleDefinition`, `G4VUserPhysicsList`, ...),
   `G4ThreadLocal` statics, `G4Cache`, and thread-private singletons all key off
   the *identity of the executing OS thread*. Geant4's `G4ThreadLocalSingleton`
   and `G4Cache` cleanup fire at thread exit / program exit.
3. **Worker initialization is per-thread and stateful.** A worker's TLS must be
   initialized *on the thread that will run it* (copying constant state from the
   fully-configured master), before any event work.

The critical consequence: **a given Geant4 worker's event processing must
always run on the same OS thread** from `initializeG4` through every `produce`
to `endRun`. TBB tasks migrate across worker threads freely, so a TBB task
cannot call Geant4 directly — the TLS a task sees depends on which TBB worker
happens to run it.

## What CMSSW does, and why it looks obscure

`OscarMTProducer` is a `stream::EDProducer` (one instance per stream = per
concurrent event slot). The obscurity is entirely `edm::ThreadHandoff
m_handoff`:

- `ThreadHandoff` spawns **one dedicated `pthread`** (with a configurable stack
  size — Geant4 needs a big stack, default 10 MiB) and parks it on a condition
  variable.
- Every Geant4 interaction is wrapped: `m_handoff.runAndWait([...]{ ... })` in
  the constructor, `beginRun`, `endRun`, `produce`, and the destructor.
- `runAndWait` ships a type-erased functor to the parked thread, wakes it,
  blocks the TBB thread until the functor completes, and rethrows any exception.

So CMSSW guarantees "same OS thread for all Geant4 calls" by owning that thread
explicitly and handing work to it. The handshaking between the master thread,
TBB threads, and Geant4 worker threads is exactly this condition-variable
handoff, plus the `ServiceRegistry::Operate guard{token}` dance to re-propagate
CMSSW services across the thread boundary, plus the `StaticRandomEngineSetUnset`
juggling of the CLHEP global engine. The algorithm writer is fully exposed to
all of it.

## The mapping to `managed_thread` / `pinned_thread_resource`

Comparing `ThreadHandoff` to `managed_thread`
(`resource_limiting_test.cpp:49-93`):

| CMSSW `ThreadHandoff` | Phlex `managed_thread` |
| --- | --- |
| dedicated `pthread m_thread` | `std::jthread thread_` |
| `m_threadHandoff` condition_variable + mutex | `work_ready_` / `slot_ready_` binary semaphores |
| `runAndWait(F&&)` ships type-erased functor, blocks caller | `execute(Args...)` sets args, releases `work_ready_`, blocks on `slot_ready_` |
| `stopThread()` / `m_stopThread` | destructor sets `gear_ = park`, releases `work_ready_` |
| rethrows `exception_ptr` | (not yet handled — see caveats) |

They are the same pattern. The difference is *who drives it*:

- In CMSSW, the producer author writes every `runAndWait(...)` call by hand.
- In Phlex, the `managed_thread` is wrapped in a `flow::resource_limiter`, and
  the `resource_limited_node` machinery acquires a token, hands the algorithm
  the `managed_thread*`, and the algorithm merely calls
  `geant4_resource->execute(i)` (`resource_limiting_test.cpp:118-125`). The
  handoff, blocking, and serialization are handled by the framework.

Crucially, `resource_limiter` gives Geant4's "N workers" model for free:
construct the limiter with N `managed_thread` tokens, and TBB admits at most N
concurrent event tasks into the Geant4 node, each pinned to a distinct
dedicated OS thread. That is a direct structural analog of `G4MTRunManager`
owning N `G4WorkerRunManager`s.

## Is it possible under the constraints? Yes, with these design points

It works because the pinned thread is a real, persistent OS thread. Geant4's
TLS, split-classes, and `G4Cache` only require thread *identity stability*,
which `std::jthread` provides for its lifetime. As long as worker G4
initialization and all subsequent event calls for that worker are routed
through the *same* `managed_thread`, Geant4 is satisfied and cannot tell the
difference between this and `G4THREADCREATE`.

To make it fully work, the design must address:

1. **Pin the whole Geant4 worker lifecycle to the token, not just `produce`.**
   The token (`managed_thread`) must run worker `initializeG4`/`beginRun`, every
   event, and `endRun`/teardown. If a token can be handed to different algorithm
   invocations that correspond to *different* Geant4 workers, the TLS will not
   match. The cleanest model: **one `managed_thread` == one Geant4 worker**, with
   its worker state initialized lazily on first `execute` (Geant4's own
   `G4WorkerRunManager` construction happens on that thread). The
   `resource_limiter` holding N of them then *is* the worker pool.
2. **Master thread / global initialization.** Geant4 requires a single
   `G4MTRunManager` created and configured before workers spawn (CMSSW's
   `OscarMTMasterThread` global cache, driven on its own handoff thread). Phlex
   needs an equivalent job-level/run-level hook that constructs and configures
   the master before the pinned worker tokens run anything. This is orthogonal
   to the per-event resource mechanism but must exist.
3. **Stack size.** `ThreadHandoff` takes a `stackSize` (default 10 MiB). Geant4
   tracking is stack-hungry. `std::jthread` gives no portable control over stack
   size, so a production `managed_thread` for Geant4 likely needs a
   pthread-based implementation (or a platform-specific attribute) rather than
   raw `std::jthread`.
4. **Exception propagation.** `ThreadHandoff::runAndWait` captures and rethrows
   `exception_ptr`. The current `managed_thread::execute` does not; a Geant4
   exception on the pinned thread would be lost or terminate. This must be
   added.
5. **Thread-exit cleanup ordering.** Geant4 registers cleanup via `G4AutoDelete`
   / `G4ThreadLocalSingleton` that fires at thread termination. The pinned
   thread must run Geant4 worker teardown *before* the `jthread` joins, and
   teardown must itself run on that thread. `managed_thread`'s destructor
   currently just parks and stops; a Geant4 variant needs a "run teardown
   functor, then stop" step.
6. **Service/context re-propagation across the boundary.** CMSSW re-establishes
   its `ServiceRegistry` inside each `runAndWait` (`Operate guard{token}`) and
   manages the CLHEP global RNG engine per event. Phlex's analog: whatever
   ambient context an algorithm needs (RNG, logging context) must be
   re-established inside the pinned execution, or the pinned-thread body must be
   given it. This is a framework responsibility, not the algorithm writer's —
   which is precisely the win.

## Bottom line

The `pinned_thread_resource` design is compatible with Geant4 and achieves the
stated goal: algorithm writers acquire a pinned-thread token and call
`execute(...)`, with no visible TLS handshaking. It is the same core mechanism
as CMSSW's `ThreadHandoff`, but inverted — instead of every author hand-writing
`runAndWait`, the framework's `resource_limited_node` performs the handoff and
the `resource_limiter` enforces the N-worker concurrency limit. The obscurity in
CMSSW is not fundamental to Geant4; it is a consequence of `ThreadHandoff` being
a manual, per-producer construct rather than a first-class framework resource.

The non-negotiables to get right are:

1. **One pinned thread == one Geant4 worker for its entire lifecycle** (init →
   events → teardown all on that thread).
2. A **master/global init hook** for `G4MTRunManager`.
3. **Configurable stack size** (favoring a pthread-backed `managed_thread` over
   raw `std::jthread`).
4. **Exception propagation** from the pinned thread.
5. **On-thread teardown** honoring Geant4's TLS cleanup.

None of these conflict with the resource-limiter model; they are implementation
requirements of the pinned token itself.
