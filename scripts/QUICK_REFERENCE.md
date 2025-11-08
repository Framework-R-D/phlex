# Environment Setup Quick Reference

## For Most Users

### First Time Setup

```bash
# Clone repository
git clone https://github.com/Framework-R-D/phlex.git
cd phlex

# Source environment setup
. scripts/setup-env.sh

# Configure and build
cmake --preset default -S . -B build
ninja -C build

# Run tests
cd build && ctest
```

### Daily Development

```bash
# Start each session with
. scripts/setup-env.sh

# Then build as needed
ninja -C build
```

## Environment Variables (Optional)

Set these **before** sourcing `setup-env.sh` to customize behavior:

```bash
# Use custom Spack installation
export PHLEX_SPACK_ROOT=/opt/spack

# Use specific Spack environment
export PHLEX_SPACK_ENV=phlex-dev

# Use custom build directory
export PHLEX_BUILD_DIR=/tmp/phlex-build

# Set build type
export CMAKE_BUILD_TYPE=Debug
```

## Multi-Project Workspace Users

If your workspace structure is:

```text
workspace/
├── srcs/
│   └── phlex/
├── build/
└── local/
```

Then source from your workspace root:

```bash
cd /path/to/workspace
. srcs/phlex/scripts/setup-env.sh
```

## System Requirements

### Minimum Required

- CMake ≥ 3.24
- GCC/G++ or Clang/Clang++ compiler toolchain
- Git

### Recommended

- Ninja (for faster builds)
- gcov and gcovr (for GCC coverage workflows)
- lcov (for HTML coverage reports)
- llvm-profdata and llvm-cov (for Clang coverage workflows)

### Installation Examples

**Ubuntu/Debian:**

```bash
sudo apt install cmake g++ ninja-build gcovr lcov git
```

**Fedora/RHEL:**

```bash
sudo dnf install cmake gcc-c++ ninja-build gcovr lcov git
```

**macOS:**

```bash
brew install cmake gcc ninja gcovr lcov git
```

## Troubleshooting

### Spack Not Found

This is normal if you're using system packages. The script will continue with system tools.

To use Spack:

```bash
export PHLEX_SPACK_ROOT=/path/to/spack
. scripts/setup-env.sh
```

### Build Tools Missing

Install via your system package manager or Spack:

```bash
# System installation (Ubuntu)
sudo apt install cmake g++ ninja-build

# OR via Spack
spack install cmake gcc ninja
spack load cmake gcc ninja
```

### Permission Denied Creating Build Directory

```bash
# Specify writable build directory
export PHLEX_BUILD_DIR=$HOME/phlex-build
. scripts/setup-env.sh
```

## Coverage Testing

Quick coverage workflow:

```bash
# Complete coverage analysis in one command
./scripts/coverage.sh all

# View HTML report
./scripts/coverage.sh view
```

See `scripts/README.md` for detailed coverage documentation.

## Getting Help

- Script documentation: `scripts/README.md`
- Main README: `README.md`
- GitHub Issues: <https://github.com/Framework-R-D/phlex/issues>
