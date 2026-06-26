// Dynamic dispatcher from generically packaged args to any C or Python function.
//
// Note: this particular implementation is based on libffi, presumed to be for
// now the minimal dependency,  but an alternative could be based on JITing
// using Cling or even Numba's llvmlite.

#include "dyncall.hpp"
#include <stdexcept>

#include <ffi.h>

using namespace phlex::experimental;

phlex::experimental::dcarg phlex::experimental::dcarg::from_str(std::string const& stype)
{
  // only types currently used in modulewrap are added, not all ffi types
  if (stype == "bool")
    return dcarg(false);
  else if (stype == "int32_t")
    return dcarg(static_cast<std::int32_t>(0));
  else if (stype == "uint32_t")
    return dcarg(static_cast<std::uint32_t>(0));
  else if (stype == "int64_t")
    return dcarg(static_cast<ph_long_t>(0));
  else if (stype == "uint64_t")
    return dcarg(static_cast<ph_ulong_t>(0));
  else if (stype == "float")
    return dcarg(0.0f);
  else if (stype == "double")
    return dcarg(0.0);
  else if (stype == "void")
    return dcarg{};

  throw std::invalid_argument("unknown type string: " + stype);
}

void* phlex::experimental::dcarg::value_ptr()
{
  return std::visit(
    [](auto& val) -> void* {
      using T = std::decay_t<decltype(val)>;
      if constexpr (std::is_same_v<T, std::monostate>) {
        return nullptr;
      } else {
        return static_cast<void*>(&val);
      }
    },
    m_value);
}

namespace {
  static ffi_type* get_ffi_type(dcarg const& d)
  {
    return std::visit(
      [](auto&& val) -> ffi_type* {
        using T = std::decay_t<decltype(val)>;

        // there are duplicate bodies here b/c bool is represented by uint8,
        // just as uint8 is, there being no bool in C; the code is cleaner
        // with each type on its own line, however, rather than combining the
        // two in a single predicate as a special case
        // NOLINTBEGIN(bugprone-branch-clone)
        if constexpr (std::is_same_v<T, std::monostate>)
          return &ffi_type_void;
        else if constexpr (std::is_same_v<T, void*>)
          return &ffi_type_pointer;
        else if constexpr (std::is_same_v<T, bool>)
          return &ffi_type_uint8;
        else if constexpr (std::is_same_v<T, std::int8_t>)
          return &ffi_type_sint8;
        else if constexpr (std::is_same_v<T, std::uint8_t>)
          return &ffi_type_uint8;
        else if constexpr (std::is_same_v<T, std::int16_t>)
          return &ffi_type_sint16;
        else if constexpr (std::is_same_v<T, std::uint16_t>)
          return &ffi_type_uint16;
        else if constexpr (std::is_same_v<T, std::int32_t>)
          return &ffi_type_sint32;
        else if constexpr (std::is_same_v<T, std::uint32_t>)
          return &ffi_type_uint32;
        else if constexpr (std::is_same_v<T, ph_long_t>)
          return &ffi_type_sint64;
        else if constexpr (std::is_same_v<T, ph_ulong_t>)
          return &ffi_type_uint64;
        else if constexpr (std::is_same_v<T, float>)
          return &ffi_type_float;
        else if constexpr (std::is_same_v<T, double>)
          return &ffi_type_double;
        // NOLINTEND(bugprone-branch-clone)
      },
      d.m_value);
  }
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
    t[i] = get_ffi_type(a);
    p[i] = a.value_ptr();
  }

  ffi_cif cif;
  ffi_status status;
  if (0 < var_offset)
    status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, var_offset, N, get_ffi_type(result), t.get());
  else
    status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, N, get_ffi_type(result), t.get());

  if (status)
    throw std::runtime_error("ffi prep failed");

  ffi_call(&cif, (void (*)())fn, result.value_ptr(), p.get());
  // NOLINTEND
}
