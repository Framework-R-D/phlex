// Copyright (C) 2025 ...

#include "root_thread_safety.hpp"

#include <TROOT.h>

#include <mutex>

namespace form::detail::experimental {

  void ensure_root_thread_safety()
  {
    static std::once_flag enabled;
    std::call_once(enabled, [] { ROOT::EnableThreadSafety(); });
  }

} // namespace form::detail::experimental
