// Copyright (C) 2025 ...

#include "root_thread_safety.hpp"

#include <TROOT.h>

namespace form::detail::experimental {

  void ensure_root_thread_safety() { ROOT::EnableThreadSafety(); }

} // namespace form::detail::experimental
