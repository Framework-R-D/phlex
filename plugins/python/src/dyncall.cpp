// Dynamic dispatcher from generically packaged args to any C or Python function.
//
// Note: this particular implementation is based on libffi, presumed to be for
// now the minimal dependency,  but an alternative could be based on JITing
// using Cling or even Numba's llvmlite.

#include "dyncall.hpp"
#include <stdexcept>
#include <string>

#include <ffi.h>


using namespace phlex::experimental;

namespace phlex::experimental {
  static ffi_type* const ffi_t[] = {
    &ffi_type_pointer,
    &ffi_type_uint8,
    &ffi_type_sint8,
    &ffi_type_uint8,
    &ffi_type_sint16,
    &ffi_type_uint16,
    &ffi_type_sint32,
    &ffi_type_uint32,
    &ffi_type_sint64,
    &ffi_type_uint64,
    &ffi_type_float,
    &ffi_type_double,
    &ffi_type_void,
  };
} // namespace phlex::experimental

phlex::experimental::edctype phlex::experimental::str2edctype(const std::string& stype)
{
  if (stype == "bool")
    return edctype::Bool;
  else if (stype == "int32_t")
    return edctype::I32;
  else if (stype == "uint32_t")
    return edctype::U32;
  else if (stype == "int64_t")
    return edctype::I64;
  else if (stype == "uint64_t")
    return edctype::U64;
  else if (stype == "float")
    return edctype::F32;
  else if (stype == "double")
    return edctype::F64;
  return edctype::Void;
}

void phlex::experimental::dyncall(void* fn, dcarg& result, dcargs_t& args, int var_offset)
{
  // Perform a dynamic call of function fn, taking arguments `args` and returning
  // `result`. Set `var_offset` to the appropriate number of positional arguments
  // if the other arguments are variational.

  // Except for the memory management unique_ptrs, this code is essentially C,
  // because libffi is, and that yields a plethora of warnings from clang-tidy,
  // none of which warrant actual changes.
  // NOLINTBEGIN
  std::size_t N = (std::size_t)args.size();

  auto t = std::make_unique<ffi_type*[]>(N);
  auto p = std::make_unique<void*[]>(N);

  for (dcargs_t::size_type i = 0; i < N; ++i) {
    auto& a = args[i];
    t[i] = ffi_t[(int)a.m_type];
    p[i] = a.value_ptr();
  }

  ffi_cif cif;
  ffi_status status;
  if (0 < var_offset)
    status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, var_offset, N, ffi_t[(int)result.m_type], t.get());
  else
    status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, N, ffi_t[(int)result.m_type], t.get());

  if (status)
    throw std::runtime_error("ffi prep failed");

  ffi_call(&cif, (void (*)())fn, result.value_ptr(), p.get());
  // NOLINTEND
}
