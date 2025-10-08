#!/bin/bash

# Provides convenient commands for managing code coverage in the Phlex project

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_SOURCE="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

COVERAGE_TESTS_READY=0
LAST_STALE_SOURCE=""
LAST_STALE_GCNO=""

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
    echo "Important: Coverage data workflow"
    echo "  1. After modifying source code, you MUST rebuild before generating reports:"
    echo "       $0 setup test html        # Rebuild → test → generate HTML"
    echo "       $0 all                    # Complete workflow (recommended)"
    echo "  2. Coverage data (.gcda/.gcno files) become stale when source files change."
    echo "  3. Stale data causes 'source file is newer than notes file' errors."
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
    echo "  BUILD_DIR        Override build directory (default: $BUILD_DIR)"
    echo ""
    echo "Examples:"
    echo "  $0 all                          # Complete workflow (recommended)"
    echo "  $0 setup test html              # Manual workflow after code changes"
    echo "  $0 xml && $0 upload             # Generate and upload"
}

check_build_dir() {
    detect_build_environment
    if [[ ! -d "$BUILD_DIR" ]]; then
        error "Build directory not found: $BUILD_DIR"
        error "Run '$0 setup' to create the build directory"
        exit 1
    fi
}

# Determine whether coverage instrumentation (.gcno files) is missing or stale
find_stale_instrumentation() {
    local build_dir="${1:-$BUILD_DIR}"
    local source_dir="${2:-$PROJECT_SOURCE}"

    LAST_STALE_SOURCE=""
    LAST_STALE_GCNO=""

    if [[ ! -d "$build_dir" ]]; then
        return 2
    fi

    local gcno_found=0
    while IFS= read -r gcno_file; do
        gcno_found=1
        local base_name
        base_name=$(basename "$gcno_file" .gcno)
        local source_file
        source_file=$(find -L "$source_dir" -type f -name "$base_name" 2>/dev/null | head -1)
        if [[ -n "$source_file" && -f "$source_file" && "$source_file" -nt "$gcno_file" ]]; then
            LAST_STALE_SOURCE="$source_file"
            LAST_STALE_GCNO="$gcno_file"
            return 1
        fi
    done < <(find "$build_dir" -name "*.gcno" -type f 2>/dev/null)

    if [[ $gcno_found -eq 0 ]]; then
        return 2
    fi

    return 0
}

ensure_coverage_configured() {
    detect_build_environment

    local need_setup=0
    if [[ ! -d "$BUILD_DIR" ]] || [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        need_setup=1
    else
        local build_type
        build_type=$(grep "^CMAKE_BUILD_TYPE:" "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
        local coverage_enabled
        coverage_enabled=$(grep "^ENABLE_COVERAGE:" "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
        if [[ "$build_type" != "Coverage" || "$coverage_enabled" != "ON" ]]; then
            warn "Coverage build cache not configured correctly (BUILD_TYPE=$build_type, ENABLE_COVERAGE=$coverage_enabled)"
            need_setup=1
        else
            find_stale_instrumentation "$BUILD_DIR" "$PROJECT_SOURCE"
            local instrumentation_status=$?
            if [[ $instrumentation_status -eq 1 ]]; then
                warn "Coverage instrumentation is stale; rebuilding before continuing"
                need_setup=1
            elif [[ $instrumentation_status -eq 2 ]]; then
                warn "No coverage instrumentation (.gcno) files detected; running setup"
                need_setup=1
            fi
        fi
    fi

    if [[ $need_setup -eq 1 ]]; then
        log "Ensuring coverage build is configured..."
        setup_coverage
        COVERAGE_TESTS_READY=0
    fi
}

run_tests_internal() {
    local mode="${1:-manual}"

    check_build_dir

    if [[ "$mode" == "auto" ]]; then
        log "Coverage data missing or stale; running tests automatically..."
    else
        log "Running tests with coverage..."
    fi

    (cd "$BUILD_DIR" && ctest -j "$(nproc)" --output-on-failure)

    if [[ "$mode" == "auto" ]]; then
        success "Automatic test run completed!"
    else
        success "Tests completed!"
    fi

    COVERAGE_TESTS_READY=1
}

ensure_tests_current() {
    if [[ "${COVERAGE_TESTS_READY:-0}" == "1" ]]; then
        return 0
    fi

    ensure_coverage_configured

    check_coverage_freshness
    local freshness_status=$?

    case "$freshness_status" in
        0)
            COVERAGE_TESTS_READY=1
            return 0
            ;;
        1)
            find_stale_instrumentation "$BUILD_DIR" "$PROJECT_SOURCE"
            local instrumentation_status=$?
            if [[ $instrumentation_status -eq 2 ]]; then
                warn "Coverage instrumentation missing; rebuilding before running tests..."
                setup_coverage
                COVERAGE_TESTS_READY=0
            fi
            run_tests_internal "auto"
            ;;
        2)
            warn "Coverage instrumentation is stale; rebuilding before running tests..."
            setup_coverage
            COVERAGE_TESTS_READY=0
            run_tests_internal "auto"
            ;;
    esac

    check_coverage_freshness
    freshness_status=$?
    if [[ $freshness_status -ne 0 ]]; then
        error "Coverage data is still not fresh after rebuilding and running tests."
        exit 1
    fi

    COVERAGE_TESTS_READY=1
}

# Check if coverage instrumentation is stale (source files newer than .gcno files)
# .gcno files are generated at compile-time, so if sources are newer, we need to rebuild
check_coverage_freshness() {
    local source_dir="${1:-$PROJECT_SOURCE}"
    local build_dir="${2:-$BUILD_DIR}"

    # Check if any .gcno files exist (compile-time coverage instrumentation)
    local gcno_count=$(find "$build_dir" -name "*.gcno" -type f 2>/dev/null | wc -l)
    if [[ $gcno_count -eq 0 ]]; then
        warn "No coverage instrumentation files (.gcno) found in $build_dir"
        warn "Coverage commands will configure and rebuild instrumentation automatically."
        return 1
    fi

    # Check if any .gcda files exist (runtime coverage data)
    local gcda_count=$(find "$build_dir" -name "*.gcda" -type f 2>/dev/null | wc -l)
    if [[ $gcda_count -eq 0 ]]; then
        warn "No coverage data files (.gcda) found in $build_dir"
        warn "Coverage commands will run the test suite automatically to populate data."
        return 1
    fi

    # Find source files that are newer than their corresponding .gcno files
    # This indicates the source was modified after compilation
    local stale_count=0
    local stale_example=""
    local stale_example_gcno=""

    while IFS= read -r gcno_file; do
        # Get the source file path from .gcno file
        # .gcno files are named like: path/to/CMakeFiles/target.dir/source.cpp.gcno
        local base_name=$(basename "$gcno_file" .gcno)

        # Try to find the corresponding source file
        # Follow symlinks in source dir to handle symlinked source trees
        local source_file=$(find -L "$source_dir" -type f -name "$base_name" 2>/dev/null | head -1)

        if [[ -n "$source_file" && -f "$source_file" ]]; then
            # Check if source file is newer than .gcno file (compile-time instrumentation)
            if [[ "$source_file" -nt "$gcno_file" ]]; then
                stale_count=$((stale_count + 1))
                if [[ -z "$stale_example" ]]; then
                    stale_example="$source_file"
                    stale_example_gcno="$gcno_file"
                fi
            fi
        fi
    done < <(find "$build_dir" -name "*.gcno" -type f 2>/dev/null)

    if [[ $stale_count -gt 0 ]]; then
        local source_time=$(stat -c %Y "$stale_example" 2>/dev/null || stat -f %m "$stale_example" 2>/dev/null)
        local gcno_time=$(stat -c %Y "$stale_example_gcno" 2>/dev/null || stat -f %m "$stale_example_gcno" 2>/dev/null)
        warn "Coverage instrumentation is STALE! $stale_count source file(s) modified since last build."
        warn "Example modified file: $stale_example"
        warn "  Source timestamp:       $(date -d @${source_time} '+%Y-%m-%d %H:%M:%S' 2>/dev/null || date -r ${source_time} '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo 'unknown')"
        warn "  Instrumentation timestamp: $(date -d @${gcno_time} '+%Y-%m-%d %H:%M:%S' 2>/dev/null || date -r ${gcno_time} '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo 'unknown')"
        return 2
    fi

    return 0
}

# Configure coverage build and rebuild if instrumentation is stale
setup_coverage() {
    detect_build_environment

    COVERAGE_TESTS_READY=0

    # Source the environment setup script to ensure proper paths
    if [[ -f "$WORKSPACE_ROOT/setup-env.sh" ]]; then
        log "Sourcing environment setup..."
        source "$WORKSPACE_ROOT/setup-env.sh"
    fi

    log "Setting up coverage build..."
    log "Source root: $SOURCE_ROOT"
    log "Build directory: $BUILD_DIR"

    # Check if we need to reconfigure or clean rebuild
    local needs_reconfigure=false
    local needs_clean=false

    if [[ -d "$BUILD_DIR" ]]; then
        # Check if CMake is configured correctly for coverage
        if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
            local build_type=$(grep "^CMAKE_BUILD_TYPE:" "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
            local coverage_enabled=$(grep "^ENABLE_COVERAGE:" "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)

            if [[ "$build_type" != "Coverage" ]] || [[ "$coverage_enabled" != "ON" ]]; then
                warn "CMake not configured for coverage (BUILD_TYPE=$build_type, ENABLE_COVERAGE=$coverage_enabled)"
                needs_reconfigure=true
                needs_clean=true
            fi
        else
            warn "CMakeCache.txt not found - needs configuration"
            needs_reconfigure=true
        fi

        # Check if any source files are newer than their .gcno files (stale instrumentation)
        # Always check this, regardless of whether CMake config is correct
        local gcno_count=$(find "$BUILD_DIR" -name "*.gcno" -type f 2>/dev/null | wc -l)
        log "Found $gcno_count .gcno files to check for staleness"
        if [[ $gcno_count -gt 0 ]]; then
            find_stale_instrumentation "$BUILD_DIR" "$PROJECT_SOURCE"
            local instrumentation_status=$?
            if [[ $instrumentation_status -eq 1 ]]; then
                warn "Detected source file newer than instrumentation: $LAST_STALE_SOURCE"
                needs_clean=true
                needs_reconfigure=true
            fi
            log "Staleness check complete (needs_clean=$needs_clean)"
        fi
    else
        log "Build directory does not exist - will create"
        needs_reconfigure=true
    fi

    # Clean build if needed
    if [[ "$needs_clean" == "true" ]]; then
        warn "Forcing clean rebuild..."
        rm -rf "$BUILD_DIR"
        needs_reconfigure=true
    fi

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Reconfigure if needed
    if [[ "$needs_reconfigure" == "true" ]]; then
        log "Configuring CMake for coverage build..."

        # Common CMake configuration flags
        CMAKE_COMMON_FLAGS=(
            -G Ninja
            -DCMAKE_BUILD_TYPE=Coverage
            -DENABLE_COVERAGE=ON
            -DPHLEX_USE_FORM=ON
            -DFORM_USE_ROOT_STORAGE=ON
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
            -S "$SOURCE_ROOT"
            -B "$BUILD_DIR"
        )

        # Check if we have CMake presets available
        if [[ -f "$SOURCE_ROOT/CMakePresets.json" ]]; then
            log "Using CMake presets with Ninja generator..."
            cmake --preset default "${CMAKE_COMMON_FLAGS[@]}" || {
                error "CMake configuration failed"
                exit 1
            }
        else
            log "Configuring CMake with Ninja generator..."
            cmake "${CMAKE_COMMON_FLAGS[@]}" || {
                error "CMake configuration failed"
                exit 1
            }
        fi
    else
        log "CMake already configured for coverage - skipping reconfiguration"
    fi

    log "Building project..."
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" || {
        error "Build failed"
        exit 1
    }

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
    ensure_coverage_configured
    run_tests_internal "manual"
}

generate_xml() {
    ensure_tests_current
    check_build_dir
    log "Generating XML coverage report..."
    cd "$BUILD_DIR"

    # Check if coverage data files exist
    if ! find "$BUILD_DIR" -name "*.gcda" | head -1 | grep -q .; then
        error "Expected coverage data files after ensuring tests, but none were found."
        error "This indicates coverage tests failed to produce data."
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
        # Show verification information like the CI workflow does
        success "Coverage XML generated successfully"
        log "Coverage XML size: $(wc -c < "$output_file") bytes"
        log "Source paths in coverage.xml:"
        grep -o '<source>.*</source>' "$output_file" | head -5 | sed 's/^/  /'

        # Normalize paths so tooling (e.g., Codecov, VS Code coverage gutters) can locate sources
        log "Normalizing coverage XML paths for tooling compatibility..."
        if ! python3 "$SCRIPT_DIR/normalize_coverage_xml.py" \
            --repo-root "$WORKSPACE_ROOT" \
            --coverage-root "$WORKSPACE_ROOT" \
            --coverage-alias "$SOURCE_ROOT" \
            --source-dir "$WORKSPACE_ROOT" \
            "$output_file"; then
            error "Failed to normalize coverage XML. Adjust filters/excludes and retry."
            exit 1
        fi

        success "XML coverage report generated: $output_file"

        # Optionally copy to workspace root for easier access
        cp "$output_file" "$WORKSPACE_ROOT/coverage.xml"
        log "Coverage XML also available at: $WORKSPACE_ROOT/coverage.xml"
    else
        error "Failed to generate XML coverage report"
        error "coverage.xml not found in $BUILD_DIR"
        ls -la "$BUILD_DIR"/*.xml 2>/dev/null || error "No XML files found in build directory"
        exit 1
    fi
}

generate_html() {
    ensure_tests_current
    check_build_dir
    log "Generating HTML coverage report..."
    cd "$BUILD_DIR"

    # Check if coverage data files exist
    if ! find "$BUILD_DIR" -name "*.gcda" | head -1 | grep -q .; then
        error "Expected coverage data files after ensuring tests, but none were found."
        error "This indicates coverage tests failed to produce data."
        exit 1
    fi

    cmake --build "$BUILD_DIR" --target coverage-html || warn "HTML generation failed (lcov issues), but continuing..."

    if [[ -d coverage-html ]]; then
        success "HTML coverage report generated: $BUILD_DIR/coverage-html/"

        # Normalize and copy the final .info file for VS Code Coverage Gutters
        if [[ -f coverage.info.final ]]; then
            log "Normalizing LCOV coverage paths for editor tooling..."
            if ! python3 "$SCRIPT_DIR/normalize_coverage_lcov.py" \
                --repo-root "$WORKSPACE_ROOT" \
                --coverage-root "$WORKSPACE_ROOT" \
                --coverage-alias "$PROJECT_SOURCE" \
                "$BUILD_DIR/coverage.info.final"; then
                error "Failed to normalize LCOV coverage report. Adjust filters/excludes and retry."
                exit 1
            fi

            log "Copying lcov.info to workspace root for VS Code Coverage Gutters..."
            cp coverage.info.final "$WORKSPACE_ROOT/lcov.info"
            success "Coverage info file available at: $WORKSPACE_ROOT/lcov.info"

            # Maintain legacy coverage.info for downstream tooling (e.g., Codecov uploads)
            cp coverage.info.final "$WORKSPACE_ROOT/coverage.info"
        fi
    else
        warn "HTML coverage report not available (lcov dependency issues)"
    fi
}

show_summary() {
    ensure_tests_current
    check_build_dir
    log "Generating coverage summary..."
    cd "$BUILD_DIR"

    # Check if coverage data files exist
    if ! find "$BUILD_DIR" -name "*.gcda" | head -1 | grep -q .; then
        error "Expected coverage data files after ensuring tests, but none were found."
        exit 1
    fi

    # Use gcovr directly for terminal-friendly summary output
    # (The CMake targets use gcovr/lcov for XML/HTML reports)
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

    # Resolve CMake paths to physical paths (they may contain symlinks)
    local phlex_binary_physical="$(cd "$phlex_binary_dir" && pwd -P)"
    local phlex_source_physical="$(cd "$phlex_source_dir" && pwd -P)"

    # Build up the same filter set used by the CMake coverage target so manual
    # summaries match CI and IDE overlays. Capture both the logical paths that
    # CMake reports (which may include workspace symlinks) and the physical
    # paths that gcov emits after resolving symlinks.
    local -a gcovr_filter_paths=("$phlex_source_dir" "$phlex_binary_dir")

    if [[ "$phlex_source_physical" != "$phlex_source_dir" ]]; then
        gcovr_filter_paths+=("$phlex_source_physical")
    fi

    if [[ "$phlex_binary_physical" != "$phlex_binary_dir" ]]; then
        gcovr_filter_paths+=("$phlex_binary_physical")
    fi

    # Deduplicate while preserving order.
    declare -A _seen_filter
    local -a gcovr_filter_args=()
    local filter_path
    for filter_path in "${gcovr_filter_paths[@]}"; do
        if [[ -z "$filter_path" || -n "${_seen_filter[$filter_path]:-}" ]]; then
            continue
        fi
        _seen_filter[$filter_path]=1

        local escaped_path
        escaped_path="$(echo "$filter_path" | sed 's|[][$*.^\\]|\\&|g')"
        gcovr_filter_args+=("--filter" "${escaped_path}/.*")
        log "gcovr filter: ${escaped_path}/.*"
    done

    if [[ ${#gcovr_filter_args[@]} -eq 0 ]]; then
        error "No gcovr filter paths were generated"
        exit 1
    fi

    # Workaround for GCC bug #120319: "Unexpected number of branch outcomes and line coverage for C++ programs"
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=120319
    # Modern GCC (15.2+) has a regression causing negative hits in gcov output for complex C++ templates
    # Use WORKSPACE_ROOT (symlink) for --root to generate relative paths
    # Use physical filters because gcovr internally resolves paths to physical
    # Filter to include only files from phlex_BINARY_DIR and phlex_SOURCE_DIR (from CMake cache)
    # Exclude test files and external dependencies (from /scratch)
    (cd "$WORKSPACE_ROOT" && gcovr --root "$WORKSPACE_ROOT" \
        --object-directory "$BUILD_DIR" \
        "${gcovr_filter_args[@]}" \
        --exclude '.*/test/.*' \
        --exclude '.*/_deps/.*' \
        --exclude '.*/external/.*' \
        --exclude '.*/third[-_]?party/.*' \
        --exclude '.*/boost/.*' \
        --exclude '.*/tbb/.*' \
        --exclude '/usr/.*' \
        --exclude '/opt/.*' \
        --exclude '/scratch/.*' \
        --gcov-ignore-parse-errors=negative_hits.warn_once_per_file \
        --gcov-ignore-errors=no_working_dir_found \
        --print-summary)
}

view_html() {
    ensure_tests_current
    check_build_dir

    if [[ ! -d "$BUILD_DIR/coverage-html" ]]; then
        log "HTML coverage report not found. Generating it now..."
        generate_html
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

    log "Ensuring coverage XML paths are normalized before upload..."
    if ! python3 "$SCRIPT_DIR/normalize_coverage_xml.py" \
        --repo-root "$WORKSPACE_ROOT" \
        --coverage-root "$WORKSPACE_ROOT" \
        --coverage-alias "$SOURCE_ROOT" \
        --source-dir "$WORKSPACE_ROOT" \
        "$BUILD_DIR/coverage.xml"; then
        error "Coverage XML failed normalization. Investigate filters/excludes before uploading."
        exit 1
    fi

    log "Coverage XML source roots after normalization:"
    grep -o '<source>.*</source>' "$BUILD_DIR/coverage.xml" | head -5 | sed 's/^/  /'

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

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h|help)
            usage
            exit 0
            ;;
        -*)
            error "Unknown option: $1"
            echo ""
            usage
            exit 1
            ;;
        *)
            # Not an option, must be a command
            break
            ;;
    esac
done

# Process all commands in sequence
for cmd in "$@"; do
    execute_command "$cmd"
done
