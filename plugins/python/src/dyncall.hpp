#ifndef phlex_python_dyncall_hpp
#define phlex_python_dyncall_hpp

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

#include <cstdint>
#include <memory>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
// This is a temporary workaround until we have a solution for handling translation of types
// between C++ and Python.
typedef long ph_long_t;
typedef unsigned long ph_ulong_t;
#else
typedef std::int64_t ph_long_t;
typedef std::uint64_t ph_ulong_t;
#endif

namespace phlex::experimental {

  enum class edctype : std::uint8_t {
    Ptr = 0,
    Bool,
    I8,
    U8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,
    F32,
    F64,
    Void
  };

  // convenience mapper of human-readable string to edctype
  edctype str2edctype(const std::string& stype);

  struct dcarg {
    union {
      void* m_ptr;
      std::uint8_t m_bool; // stored as 0 or 1
      std::int8_t m_int8;
      std::uint8_t m_uint8;
      std::int16_t m_int16;
      std::uint16_t m_uint16;
      std::int32_t m_int;
      std::uint32_t m_uint;
      ph_long_t m_long;
      ph_ulong_t m_ulong;
      float m_float;
      double m_double;
    };
    edctype m_type;

    // factory-style constructors to guarante value/typecode match
    dcarg() : m_type(edctype::Void) { m_ptr = nullptr; }
    explicit dcarg(void* v) : m_type(edctype::Ptr) { m_ptr = v; }
    explicit dcarg(bool v) : m_type(edctype::Bool) { m_bool = v ? 1 : 0; }
    explicit dcarg(std::int8_t v) : m_type(edctype::I8) { m_int8 = v; }
    explicit dcarg(std::uint8_t v) : m_type(edctype::U8) { m_uint8 = v; }
    explicit dcarg(std::int16_t v) : m_type(edctype::I16) { m_int16 = v; }
    explicit dcarg(std::uint16_t v) : m_type(edctype::U16) { m_uint16 = v; }
    explicit dcarg(std::int32_t v) : m_type(edctype::I32) { m_int = v; }
    explicit dcarg(std::uint32_t v) : m_type(edctype::U32) { m_uint = v; }
    explicit dcarg(ph_long_t v) : m_type(edctype::I64) { m_long = v; }
    explicit dcarg(ph_ulong_t v) : m_type(edctype::U64) { m_ulong = v; }
    explicit dcarg(float v) : m_type(edctype::F32) { m_float = v; }
    explicit dcarg(double v) : m_type(edctype::F64) { m_double = v; }

    // convenience null-constructor from enumerated type
    explicit dcarg(edctype etype) : m_type(etype)
    {
      if (etype == edctype::Bool) m_bool = 0;
      else if (etype == edctype::I8) m_int8 = 0;
      else if (etype == edctype::U8) m_uint8 = 0;
      else if (etype == edctype::I16) m_int16 = 0;
      else if (etype == edctype::U16) m_uint16 = 0;
      else if (etype == edctype::I32) m_int = 0;
      else if (etype == edctype::U32) m_uint = 0;
      else if (etype == edctype::I64) m_long = 0l;
      else if (etype == edctype::U64) m_ulong = 0ul;
      else if (etype == edctype::F32) m_float = 0.f;
      else if (etype == edctype::F64) m_double = 0.;
      else m_ptr = nullptr;
    }

    // pointer access to payload
    void* value_ptr()
    {
      switch (m_type) {
      case edctype::Ptr:
        return &m_ptr;
      case edctype::Bool:
        return &m_bool;
      case edctype::I8:
        return &m_int8;
      case edctype::U8:
        return &m_uint8;
      case edctype::I16:
        return &m_int16;
      case edctype::U16:
        return &m_uint16;
      case edctype::I32:
        return &m_int;
      case edctype::U32:
        return &m_uint;
      case edctype::I64:
        return &m_long;
      case edctype::U64:
        return &m_ulong;
      case edctype::F32:
        return &m_float;
      case edctype::F64:
        return &m_double;
      case edctype::Void:
        return nullptr;
      default:
        throw std::invalid_argument("unknown edctype");
      }
    }
  };

  typedef std::vector<dcarg> dcargs_t;

  void dyncall(void* fn, dcarg& result, dcargs_t& args, int var_offset = -1);

} // phlex::experimental

#endif // phlex_python_dyncall_hpp
