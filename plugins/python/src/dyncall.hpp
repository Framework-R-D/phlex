#ifndef PLUGINS_PYTHON_SRC_DYNCALL_HPP
#define PLUGINS_PYTHON_SRC_DYNCALL_HPP

// =======================================================================================
//
// Dynamic dispatcher from generically packaged args to any C or Python function.
//
// Design rationale
// ================
//
// Python code is inserted in the Phlex execution graph using generic types to avoid a
// combinatorial explosion of types. This way, all template instantiations can be done at
// compile time. Callback wrappers are then needed to either pack from generic to Python
// or to unpack from generic to C/C++ and perform the call. This dynamic dispatcher
// provides that functionality.
//
// =======================================================================================

#include "Python.h" // for PyObject* get<> specialization only

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
// This is a temporary workaround until we have a solution for handling translation of types
// between C++ and Python.
using ph_long_t = long;
using ph_ulong_t = unsigned long;
#else
using ph_long_t = std::int64_t;
using ph_ulong_t = std::uint64_t;
#endif

namespace phlex::experimental {

  struct dcarg {
    using ffi_variant_type = std::variant<std::monostate, // void (default)
                                          void*,
                                          bool,
                                          std::int8_t,
                                          std::uint8_t,
                                          std::int16_t,
                                          std::uint16_t,
                                          std::int32_t,
                                          std::uint32_t,
                                          ph_long_t,
                                          ph_ulong_t,
                                          float,
                                          double>;

    ffi_variant_type m_value;

    // convenience mapper of human-readable string to dcarg
    static dcarg from_str(std::string const& stype);

    // factory-style constructors to guarantee value/type match
    dcarg() : m_value(std::monostate{}) {}
    explicit dcarg(void* v) : m_value(v) {}
    explicit dcarg(bool v) : m_value(v) {}
    explicit dcarg(std::int8_t v) : m_value(v) {}
    explicit dcarg(std::uint8_t v) : m_value(v) {}
    explicit dcarg(std::int16_t v) : m_value(v) {}
    explicit dcarg(std::uint16_t v) : m_value(v) {}
    explicit dcarg(std::int32_t v) : m_value(v) {}
    explicit dcarg(std::uint32_t v) : m_value(v) {}
    explicit dcarg(ph_long_t v) : m_value(v) {}
    explicit dcarg(ph_ulong_t v) : m_value(v) {}
    explicit dcarg(float v) : m_value(v) {}
    explicit dcarg(double v) : m_value(v) {}

    // pointer access to payload
    void* value_ptr();

    // value access to payload
    template <typename T>
    T get() const
    {
      return std::get<T>(m_value);
    }
  };

  // specialization to simplify a very common case
  template <>
  inline PyObject* dcarg::get<PyObject*>() const
  {
    return reinterpret_cast<PyObject*>(std::get<void*>(m_value));
  }

  using dcargs_t = std::vector<dcarg>;

  void dyncall(void* fn, dcarg& result, dcargs_t& args, int var_offset = -1);

} // phlex::experimental

#endif // PLUGINS_PYTHON_SRC_DYNCALL_HPP
