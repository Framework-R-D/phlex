#include "wrap.hpp"

#include <oneapi/tbb/enumerable_thread_specific.h>

#include <atomic>
#include <stdexcept>

namespace {

  // Python algorithms are inserted into the phlex graph, which is templated and
  // thus statically typed, using converters from C++ types to Python types for
  // the input; and the reverse for the output. These conversions require the GIL,
  // even as the individual operations are tiny, so that acquisition is rather
  // expensive, especially so since these are threads originating from C++, thus
  // each GIL acquisition will create a Python thread state that is subsequently
  // destroyed after the converter has run, only for the next converter to need
  // (and recreate) it again.
  //
  // To prevent the constant creation/destruction of Python thread states, the
  // following code creates one per thread and keeps it alive (the unmatched Save
  // in the constructor) until an explicit shutdown.
  //
  // Note 1: phlex does not currently finalize the Python interpreter, so atm.,
  // the explicit shutdown never happens. The cleanup code below is therefore
  // commented out, until there's proper finalization. (This explicit cleanup is
  // also why py_tstate below does not have a non-default destructor: releasing
  // the thread state could be called after Py_Finalize, with crashes as result.)
  //
  // Note 2: Python will clean up all thread states upon finalization. The more
  // important bits of the cleanup are the TBB container, and flagging that the
  // interpreter has finalized (or a crash may ensue on shutdown).

  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)
  // Process-wide shutdown flag and per-thread state registry; both must be mutable.
  // If the registry's allocation throws at startup, terminating is the right outcome.
  std::atomic<bool> py_tstate_finalized{false};

  tbb::enumerable_thread_specific<py_tstate,
                                  tbb::cache_aligned_allocator<py_tstate>,
                                  tbb::ets_key_per_instance>
    all_py_tstates;
  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)

  struct py_tstate {
    PyThreadState* ts_;
    py_tstate()
    {
      PyGILState_Ensure();
      // Must hold the GIL before saving the state; could set it to nullptr in
      // the member initializer, but that's just more noise.
      // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
      ts_ = PyEval_SaveThread();
      // no GIL held, but thread state kept alive with +1 refcount
    }
    ~py_tstate() = default;
    py_tstate(py_tstate const&) = delete;
    py_tstate& operator=(py_tstate const&) = delete;
    py_tstate(py_tstate&&) = delete;
    py_tstate& operator=(py_tstate&&) = delete;
  };

  inline void ensure_local_py_tstate()
  {
    if (py_tstate_finalized.load(std::memory_order_acquire)) {
      throw std::logic_error("Python GIL acquire attempt after interpreter shutdown");
    }
    all_py_tstates.local();
  }

} // unnamed namespace

phlex::experimental::py_gilraii::py_gilraii()
{
  ensure_local_py_tstate();
  // Must alive the thread state before grabbing the GIL here as that's the whole
  // point of caching it; setting it to nullptr first is just more noise.
  // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
  gil_state_ = PyGILState_Ensure();
}

phlex::experimental::py_gilraii::~py_gilraii() { PyGILState_Release(gil_state_); }

// The following should be called one day after the graph has run and
// before Py_Finalize. Currently, there's no such location in phlex. Thus,
// this code is commented out as it can not be tested in coverage as it's
// not called anywhere yet and can't be covered with tests.

//void phlex::experimental::release_py_tstates()
//{
//  // debugging aid: catches py_gillraii usage after shutdown
//  py_tstate_finalized.store(true, std::memory_order_release);
//
//  // acquire GIl to allow cleanup of Python thread states
//  PyGILState_STATE s = PyGILState_Ensure();
//  PyThreadState* self = PyThreadState_Get();
//
//  for (auto& e : all_py_tstates) {
//    if (e.ts_ == self)
//      continue;
//
//    // cleanup Python thread states
//    PyThreadState_Clear(e.ts_);  // may run Python code
//    PyThreadState_Delete(e.ts_); // free memory
//  }
//
//  PyGILState_Release(s);
//
//  // cleanup TBB thread local container
//  all_py_tstates.clear();
//}
