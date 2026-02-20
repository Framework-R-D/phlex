# Test Failure Fixes

## Issues Fixed

### 1. type_id.cpp - Missing fmt header

**Problem**: Test uses `fmt::format()` but header was not included
**Fix**: Added `#include "fmt/format.h"` to includes
**Note**: CMakeLists.txt already had `fmt::fmt` dependency

### 2. identifier.cpp - Missing Boost::json dependency

**Problem**: Test uses `boost::json::parse()` but library was not linked
**Fix**: Added `Boost::json` to LIBRARIES in CMakeLists.txt

## Files Modified

1. **test/type_id.cpp** - Added fmt/format.h include
2. **test/CMakeLists.txt** - Added Boost::json to identifier test

## Verification

All three converted tests now have correct dependencies:

- `identifier`: phlex::model, phlex::configuration, Boost::json
- `type_id`: phlex::model, fmt::fmt
- `yielding_driver`: phlex::core, TBB::tbb

## Root Cause

During conversion, I incorrectly removed necessary dependencies:

- Removed fmt from identifier (correct - it doesn't use fmt)
- Forgot to add Boost::json to identifier (incorrect - it uses boost::json)
- Forgot to include fmt header in type_id.cpp (incorrect - it uses fmt::format)
