#!/bin/bash

# Provides convenient commands for managing code coverage in the Phlex project

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_SOURCE="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function definitions
# Get absolute path (preserving symlinks - DO NOT resolve them)
get_absolute_path() {
    (cd "$1" && pwd)
}

log() {
    echo -e "${BLUE}[Coverage]${NC} $1"
}

success() {
    echo -e "${GREEN}[Coverage]${NC} $1"
}

error() {
    echo -e "${RED}[Coverage]${NC} $1" >&2
}

warn() {
    echo -e "${YELLOW}[Coverage]${NC} $1"
}

# Detect build environment and set appropriate paths
detect_build_environment() {
    # Check if we're in a multi-project structure by looking for workspace indicators
    local workspace_candidate="$(dirname "$(dirname "$PROJECT_SOURCE")")"

    # Multi-project mode indicators:
    # 1. srcs directory exists in workspace
    # 2. setup-env.sh exists in workspace (common pattern)
    # 3. build directory exists in workspace
    if [[ -d "$workspace_candidate/srcs" && (-f "$workspace_candidate/setup-env.sh" || -d "$workspace_candidate/build") ]]; then
        # Multi-project mode - phlex is part of a larger project
        SOURCE_ROOT="$(dirname "$PROJECT_SOURCE")"  # srcs directory
        WORKSPACE_ROOT="$(get_absolute_path "$workspace_candidate")"
        BUILD_DIR="${BUILD_DIR:-$WORKSPACE_ROOT/build}"
        log "Detected multi-project build mode"
        log "Multi-project source root: $SOURCE_ROOT"
        log "Project source: $PROJECT_SOURCE"
        log "Workspace root: $WORKSPACE_ROOT"
    elif [[ -f "$PROJECT_SOURCE/CMakeLists.txt" ]]; then
        # Standalone mode - building phlex directly
        SOURCE_ROOT="$PROJECT_SOURCE"
        WORKSPACE_ROOT="$(get_absolute_path "$(dirname "$PROJECT_SOURCE")")"
        BUILD_DIR="${BUILD_DIR:-$WORKSPACE_ROOT/build}"
        log "Detected standalone build mode"
        log "Project source: $PROJECT_SOURCE"
        log "Workspace root: $WORKSPACE_ROOT"
    else
        # Fallback to original logic
        WORKSPACE_ROOT="$(get_absolute_path "$(dirname "$(dirname "$PROJECT_SOURCE")")")"
        SOURCE_ROOT="$PROJECT_SOURCE"
        BUILD_DIR="${BUILD_DIR:-$WORKSPACE_ROOT/build}"
        warn "Could not detect build mode, using fallback paths"
    fi

    # Source environment setup if available
    # Try workspace-level first, then repository-level
    if [ -f "$WORKSPACE_ROOT/setup-env.sh" ]; then
        log "Sourcing workspace environment: $WORKSPACE_ROOT/setup-env.sh"
        . "$WORKSPACE_ROOT/setup-env.sh"
        if (( $? != 0 )); then
            error "unable to source workspace setup-env.sh successfully"
            exit 1
        fi
    elif [ -f "$PROJECT_SOURCE/scripts/setup-env.sh" ]; then
        log "Sourcing repository environment: $PROJECT_SOURCE/scripts/setup-env.sh"
        . "$PROJECT_SOURCE/scripts/setup-env.sh"
        if (( $? != 0 )); then
            error "unable to source repository setup-env.sh successfully"
            exit 1
        fi
    else
        warn "No setup-env.sh found - assuming environment is already configured"
        warn "Expected locations:"
        warn "  - $WORKSPACE_ROOT/setup-env.sh (workspace-level)"
        warn "  - $PROJECT_SOURCE/scripts/setup-env.sh (repository-level)"
    fi
}

usage() {
    # Initialize environment detection before showing paths
    detect_build_environment

    echo "Usage: $0 [COMMAND] [COMMAND...]"
    echo ""
    echo "Commands:"
    echo "  setup     Set up coverage build directory"
    echo "  clean     Clean coverage data files"
    echo "  test      Run tests with coverage"
    echo "  report    Generate coverage reports"
    echo "  xml       Generate XML coverage report"
    echo "  html      Generate HTML coverage report"
    echo "  view      Open HTML coverage report in browser"
    echo "  summary   Show coverage summary"
    echo "  upload    Upload coverage to Codecov"
    echo "  all       Run setup, test, and generate all reports"
    echo "  help      Show this help message"
    echo ""
    echo "Multiple commands can be specified and will be executed in sequence:"
    echo "  $0 setup test summary"
    echo "  $0 clean setup test html view"
    echo ""
    echo "Codecov Token Setup (choose one method):"
    echo "  export CODECOV_TOKEN='your-token'"
    echo "  echo 'your-token' > ~/.codecov_token && chmod 600 ~/.codecov_token"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_DIR   Override build directory (default: $BUILD_DIR)"
    echo ""
    echo "Examples:"
    echo "  $0 all                    # Complete workflow"
    echo "  $0 xml && $0 upload       # Generate and upload"
}

check_build_dir() {
    detect_build_environment
    if [[ ! -d "$BUILD_DIR" ]]; then
        error "Build directory not found: $BUILD_DIR"
        error "Run '$0 setup' to create the build directory"
        exit 1
    fi
}

setup_coverage() {
    detect_build_environment

    # Source the environment setup script to ensure proper paths
    if [[ -f "$WORKSPACE_ROOT/setup-env.sh" ]]; then
        log "Sourcing environment setup..."
        source "$WORKSPACE_ROOT/setup-env.sh"
    fi

    log "Setting up coverage build..."
    log "Source root: $SOURCE_ROOT"
    log "Build directory: $BUILD_DIR"

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Common CMake configuration flags
    CMAKE_COMMON_FLAGS=(
        -DCMAKE_BUILD_TYPE=Coverage
        -DENABLE_COVERAGE=ON
        -DPHLEX_USE_FORM=ON
        -DFORM_USE_ROOT_STORAGE=ON
        -S "$SOURCE_ROOT"
        -B "$BUILD_DIR"
    )

    # Check if we have CMake presets available
    if [[ -f "$SOURCE_ROOT/CMakePresets.json" ]]; then
        log "Using CMake presets for configuration..."
        cmake --preset default "${CMAKE_COMMON_FLAGS[@]}"
    else
        log "Configuring CMake without presets..."
        cmake "${CMAKE_COMMON_FLAGS[@]}"
    fi

    log "Building project..."
    cmake --build "$BUILD_DIR" --parallel "$(nproc)"

    success "Coverage build setup complete!"
}

clean_coverage() {
    check_build_dir
    log "Cleaning coverage data..."
    cd "$BUILD_DIR"

    cmake --build "$BUILD_DIR" --target coverage-clean

    success "Coverage data cleaned!"
}

run_tests() {
    check_build_dir
    log "Running tests with coverage..."
    cd "$BUILD_DIR"

    ctest -j "$(nproc)" --output-on-failure

    success "Tests completed!"
}

generate_xml() {
    check_build_dir
    log "Generating XML coverage report..."
    cd "$BUILD_DIR"

    # Check if coverage data files exist
    if ! find "$BUILD_DIR" -name "*.gcda" | head -1 | grep -q .; then
        error "No coverage data files found!"
        error "You need to run tests with coverage first:"
        error "  $0 test"
        error ""
        error "Or run the complete workflow:"
        error "  $0 setup && $0 test && $0 xml"
        exit 1
    fi

    # Use CMake target to generate XML report
    log "Generating XML coverage report using CMake target..."
    cd "$BUILD_DIR"

    cmake --build "$BUILD_DIR" --target coverage-xml || {
        error "Failed to generate XML coverage report"
        exit 1
    }

    # Path to the generated XML file (CMake target outputs to BUILD_DIR)
    local output_file="$BUILD_DIR/coverage.xml"

    if [[ -f "$output_file" ]]; then
        # Convert absolute paths to relative for VS Code Coverage Gutters compatibility
        # The CMake target generates the report; we just need to post-process for VS Code
        log "Converting absolute paths to relative paths for VS Code..."
        sed -i "s|filename=\"$WORKSPACE_ROOT/|filename=\"|g" "$output_file"

        # Update the source path to be the workspace root for Coverage Gutters
        sed -i "s|<source>\.</source>|<source>$WORKSPACE_ROOT</source>|g" "$output_file"

        success "XML coverage report generated: $output_file"

        # Optionally copy to workspace root for easier access
        cp "$output_file" "$WORKSPACE_ROOT/coverage.xml"
        log "Coverage XML also available at: $WORKSPACE_ROOT/coverage.xml"
    else
        error "Failed to generate XML coverage report"
        exit 1
    fi
}

generate_html() {
    check_build_dir
    log "Generating HTML coverage report..."
    cd "$BUILD_DIR"

    cmake --build "$BUILD_DIR" --target coverage-html || warn "HTML generation failed (lcov issues), but continuing..."

    if [[ -d coverage-html ]]; then
        success "HTML coverage report generated: $BUILD_DIR/coverage-html/"

        # Copy the final .info file to workspace root for VS Code Coverage Gutters
        if [[ -f coverage.info.final ]]; then
            log "Copying coverage.info to workspace root for VS Code Coverage Gutters..."
            cp coverage.info.final "$WORKSPACE_ROOT/coverage.info"
            success "Coverage info file available at: $WORKSPACE_ROOT/coverage.info"
        fi
    else
        warn "HTML coverage report not available (lcov dependency issues)"
    fi
}

show_summary() {
    check_build_dir
    log "Generating coverage summary..."
    cd "$BUILD_DIR"

    # Check if coverage data files exist
    if ! find "$BUILD_DIR" -name "*.gcda" | head -1 | grep -q .; then
        error "No coverage data files found!"
        error "You need to run tests with coverage first:"
        error "  $0 test"
        error ""
        error "Or run the complete workflow:"
        error "  $0 setup && $0 test && $0 summary"
        exit 1
    fi

    # Use gcovr directly since ninja coverage has dependency issues
    log "Using gcovr to generate coverage summary..."

    # Extract phlex-specific paths from CMake cache for accurate filtering
    local cmake_cache="$BUILD_DIR/CMakeCache.txt"
    if [[ ! -f "$cmake_cache" ]]; then
        error "CMake cache not found: $cmake_cache"
        error "Run '$0 setup' first to configure the build"
        exit 1
    fi

    local phlex_binary_dir="$(grep '^phlex_BINARY_DIR:' "$cmake_cache" | cut -d= -f2)"
    local phlex_source_dir="$(grep '^phlex_SOURCE_DIR:' "$cmake_cache" | cut -d= -f2)"

    if [[ -z "$phlex_binary_dir" || -z "$phlex_source_dir" ]]; then
        error "Could not extract phlex paths from CMake cache"
        error "CMake cache may be incomplete or from a different project"
        exit 1
    fi

    # Get physical workspace path (gcovr internally resolves symlinks)
    local workspace_physical="$(cd "$WORKSPACE_ROOT" && pwd -P)"

    # Resolve CMake paths to physical paths (they may contain symlinks)
    local phlex_binary_physical="$(cd "$phlex_binary_dir" && pwd -P)"
    local phlex_source_physical="$(cd "$phlex_source_dir" && pwd -P)"

    # Convert absolute paths to relative paths from physical workspace
    # gcovr uses physical paths internally, so filters must be relative to physical root
    local phlex_binary_rel="${phlex_binary_physical#$workspace_physical/}"
    local phlex_source_rel="${phlex_source_physical#$workspace_physical/}"

    # Escape special regex characters in paths for use in --filter
    local build_filter="$(echo "$phlex_binary_rel" | sed 's|[][$*.^\\]|\\&|g')"
    local source_filter="$(echo "$phlex_source_rel" | sed 's|[][$*.^\\]|\\&|g')"

    # Also create filters from physical paths (gcovr internally resolves to physical)
    local build_filter_physical="$(echo "$phlex_binary_physical" | sed 's|[][$*.^\\]|\\&|g')"
    local source_filter_physical="$(echo "$phlex_source_physical" | sed 's|[][$*.^\\]|\\&|g')"

    log "Build filter (phlex_BINARY_DIR): $build_filter"
    log "Source filter (phlex_SOURCE_DIR): $source_filter"

    # Workaround for GCC bug #120319: "Unexpected number of branch outcomes and line coverage for C++ programs"
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=120319
    # Modern GCC (15.2+) has a regression causing negative hits in gcov output for complex C++ templates
    # Use WORKSPACE_ROOT (symlink) for --root to generate relative paths
    # Use physical filters because gcovr internally resolves paths to physical
    # Filter to include only files from phlex_BINARY_DIR and phlex_SOURCE_DIR (from CMake cache)
    # Exclude test files and external dependencies (from /scratch)
    (cd "$WORKSPACE_ROOT" && gcovr --root "$WORKSPACE_ROOT" \
          --object-directory "$BUILD_DIR" \
          --filter "${build_filter_physical}/.*" \
          --filter "${source_filter_physical}/.*" \
          --exclude '.*/test/.*' \
          --exclude '.*/external/.*' \
          --exclude '/scratch/.*' \
          --gcov-ignore-parse-errors=negative_hits.warn_once_per_file \
          --gcov-ignore-errors=no_working_dir_found \
          --print-summary)
}

view_html() {
    check_build_dir

    if [[ ! -d "$BUILD_DIR/coverage-html" ]]; then
        warn "HTML coverage report not found. Generate it first with '$0 html'"
        exit 1
    fi

    log "Opening HTML coverage report..."
    if command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$BUILD_DIR/coverage-html/index.html"
    elif command -v open >/dev/null 2>&1; then
        open "$BUILD_DIR/coverage-html/index.html"
    else
        echo "HTML report available at: $BUILD_DIR/coverage-html/index.html"
    fi
}

upload_codecov() {
    check_build_dir

    if [[ ! -f "$BUILD_DIR/coverage.xml" ]]; then
        warn "XML coverage report not found. Generate it first with '$0 xml'"
        exit 1
    fi

    # Check for codecov CLI
    if ! command -v codecov >/dev/null 2>&1; then
        error "Codecov CLI not found. Install it first."
        echo "  curl -Os https://cli.codecov.io/latest/linux/codecov"
        echo "  chmod +x codecov && mv codecov ~/.local/bin/"
        exit 1
    fi

    cd "$BUILD_DIR"

    # Determine the Git repository root
    GIT_ROOT="$PROJECT_SOURCE"
    if [[ ! -d "$PROJECT_SOURCE/.git" ]]; then
        # Look for .git in parent directories
        GIT_ROOT="$(git -C "$PROJECT_SOURCE" rev-parse --show-toplevel 2>/dev/null || echo "$PROJECT_SOURCE")"
    fi

    # Get commit SHA
    COMMIT_SHA=$(cd "$GIT_ROOT" && git rev-parse HEAD 2>/dev/null)
    if [[ -z "$COMMIT_SHA" ]]; then
        error "Could not determine Git commit SHA"
        exit 1
    fi

    # Check for token in various locations
    CODECOV_TOKEN=""
    if [[ -n "${CODECOV_TOKEN:-}" ]]; then
        log "Using CODECOV_TOKEN environment variable"
    elif [[ -f ~/.codecov_token ]]; then
        log "Reading token from ~/.codecov_token"
        CODECOV_TOKEN=$(cat ~/.codecov_token 2>/dev/null | tr -d '\n\r ')
    elif [[ -f .codecov_token ]]; then
        log "Reading token from .codecov_token"
        CODECOV_TOKEN=$(cat .codecov_token 2>/dev/null | tr -d '\n\r ')
    else
        warn "No Codecov token found. Trying upload without token (may fail for private repos)"
        warn "Set CODECOV_TOKEN environment variable or create ~/.codecov_token file"
    fi

    log "Uploading coverage to Codecov..."
    log "Git root: $GIT_ROOT"
    log "Commit SHA: $COMMIT_SHA"
    log "Coverage file: $BUILD_DIR/coverage.xml"

    # Build codecov command
    CODECOV_CMD=(codecov upload-coverage
                 --file coverage.xml
                 --commit-sha "$COMMIT_SHA"
                 --working-dir "$GIT_ROOT")

    if [[ -n "$CODECOV_TOKEN" ]]; then
        CODECOV_CMD+=(--token "$CODECOV_TOKEN")
    fi

    # Execute upload
    if "${CODECOV_CMD[@]}"; then
        success "Coverage uploaded to Codecov successfully!"
    else
        error "Failed to upload coverage to Codecov"
        exit 1
    fi
}

run_all() {
    setup_coverage
    run_tests
    generate_xml
    generate_html
    show_summary

    success "Complete coverage analysis finished!"
    log "XML report: $BUILD_DIR/coverage.xml"
    if [[ -d "$BUILD_DIR/coverage-html" ]]; then
        log "HTML report: $BUILD_DIR/coverage-html/index.html"
    fi
}

# Execute a single command
execute_command() {
    local cmd="$1"
    case "$cmd" in
        setup)
            setup_coverage
            ;;
        clean)
            clean_coverage
            ;;
        test)
            run_tests
            ;;
        report)
            generate_xml
            generate_html
            ;;
        xml)
            generate_xml
            ;;
        html)
            generate_html
            ;;
        view)
            view_html
            ;;
        summary)
            show_summary
            ;;
        upload)
            upload_codecov
            ;;
        all)
            run_all
            ;;
        help|--help|-h)
            usage
            ;;
        *)
            error "Unknown command: $cmd"
            echo ""
            usage
            exit 1
            ;;
    esac
}

# Main command processing
if [ $# -eq 0 ]; then
    usage
    exit 0
fi

# Process all commands in sequence
for cmd in "$@"; do
    execute_command "$cmd"
done
