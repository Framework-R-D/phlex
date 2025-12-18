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

# Default build directory for coverage runs
DEFAULT_COVERAGE_BUILD_DIR="build-coverage"

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
        BUILD_DIR="${BUILD_DIR:-$WORKSPACE_ROOT/$DEFAULT_COVERAGE_BUILD_DIR}"
        log "Detected multi-project build mode"
        log "Multi-project source root: $SOURCE_ROOT"
        log "Project source: $PROJECT_SOURCE"
        log "Workspace root: $WORKSPACE_ROOT"
    elif [[ -f "$PROJECT_SOURCE/CMakeLists.txt" ]]; then
        # Standalone mode - building phlex directly
        SOURCE_ROOT="$PROJECT_SOURCE"
        WORKSPACE_ROOT="$(get_absolute_path "$(dirname "$PROJECT_SOURCE")")"
        BUILD_DIR="${BUILD_DIR:-$WORKSPACE_ROOT/$DEFAULT_COVERAGE_BUILD_DIR}"
        log "Detected standalone build mode"
        log "Project source: $PROJECT_SOURCE"
        log "Workspace root: $WORKSPACE_ROOT"
    else
        # Fallback to original logic
        WORKSPACE_ROOT="$(get_absolute_path "$(dirname "$(dirname "$PROJECT_SOURCE")")")"
        SOURCE_ROOT="$PROJECT_SOURCE"
        BUILD_DIR="${BUILD_DIR:-$WORKSPACE_ROOT/$DEFAULT_COVERAGE_BUILD_DIR}"
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

    echo "Usage: $0 [--preset <coverage-clang|coverage-gcc>] [COMMAND] [COMMAND...]"
    echo ""
    echo "Commands:"
    echo "  setup     Set up coverage build directory (configure and build)"
    echo "  clean     Clean coverage data files"
    echo "  test      Run tests with coverage instrumentation"
    echo "  report    Generate primary coverage output (clang: text summary, gcc: gcov data bundle)"
    echo "  xml       (gcc only) Generate Cobertura XML report"
    echo "  html      Generate HTML coverage report (supported for both presets)"
    echo "  view      Open HTML coverage report in browser (supported for both presets)"
    echo "  summary   Show coverage summary in the terminal"
    echo "  upload    Upload coverage report to Codecov (clang: lcov, gcc: xml)"
    echo "  all       Run setup, test, and generate all reports"
    echo "  help      Show this help message"
    echo ""
    echo "Toolchain Workflows (Presets):"
    echo ""
    echo "  The --preset flag controls which compiler toolchain is used for coverage."
    echo ""
    echo "  Clang (default): --preset coverage-clang"
    echo "    - Recommended for local development; mirrors the CI workflow."
    echo "    - Generates fast, accurate coverage data using LLVM's instrumentation."
    echo "    - Key commands: setup, test, html, view, summary"
    echo ""
    echo "  GCC: --preset coverage-gcc"
    echo "    - Use to generate XML reports for services like Codecov."
    echo "    - Uses gcov instrumentation."
    echo "    - Key commands: setup, test, xml, html, upload"
    echo ""
    echo "Notes:"
    echo "  - Default preset is 'coverage-clang' to match the CI workflow."
    echo "  - After modifying source code, you MUST rebuild before generating reports:"
    echo "       $0 setup test html        # Rebuild → test → generate HTML"
    echo "       $0 all                    # Complete workflow (recommended)"
    echo ""
    echo "Multiple commands can be specified and will be executed in sequence:"
    echo "  $0 setup test summary"
    echo "  $0 clean setup test html view"
    echo ""
    echo "Codecov Token Setup (for 'upload' command):"
    echo "  export CODECOV_TOKEN='your-token'"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_DIR        Override build directory (default: $BUILD_DIR)"
    echo ""
    echo "Examples:"
    echo "  $0 all                          # Complete workflow using clang (default)"
    echo "  $0 --preset coverage-gcc all    # Complete workflow using gcc"
    echo "  $0 setup test html view         # Manual workflow after code changes"
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
        # GCC-specific staleness check for .gcno files
        elif [[ "$COVERAGE_PRESET" == "coverage-gcc" ]]; then
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

    # For Clang, set the LLVM_PROFILE_FILE env var to collect raw profile data
    # in a centralized location, mirroring the CI workflow.
    if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
        local PROFILE_ROOT="$BUILD_DIR/test/profraw"
        log "Cleaning LLVM profile directory: $PROFILE_ROOT"
        rm -rf "$PROFILE_ROOT"
        mkdir -p "$PROFILE_ROOT"
        export LLVM_PROFILE_FILE="$PROFILE_ROOT/%m-%p.profraw"
        log "LLVM_PROFILE_FILE set to: $LLVM_PROFILE_FILE"
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

    # For Clang, the workflow is much simpler than for GCC. We don't have the
    # complex .gcno/.gcda staleness checks. We rely on ensure_coverage_configured
    # to ensure the coverage build is present; run the `setup` command first if
    # the coverage build has not been configured yet, then run the tests.
    if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
        ensure_coverage_configured
        # If tests haven't been run in this session, run them to generate .profraw
        if [[ "${COVERAGE_TESTS_READY:-0}" != "1" ]]; then
            run_tests_internal "auto"
        fi
        COVERAGE_TESTS_READY=1
        return 0
    fi

    # --- GCC-specific logic below ---
    ensure_coverage_configured

    check_coverage_freshness
    local freshness_status=$?

    case "$freshness_status" in
        0) # Fresh
            COVERAGE_TESTS_READY=1
            return 0
            ;;
        1) # Missing .gcda files
            find_stale_instrumentation "$BUILD_DIR" "$PROJECT_SOURCE"
            local instrumentation_status=$?
            if [[ $instrumentation_status -eq 2 ]]; then
                warn "Coverage instrumentation missing; rebuilding before running tests..."
                setup_coverage
                COVERAGE_TESTS_READY=0
            fi
            run_tests_internal "auto"
            ;;
        2) # Stale .gcno files
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

        local preset_name="${COVERAGE_PRESET:-coverage-clang}"
        log "Using CMake coverage preset: $preset_name"
        cmake --preset "$preset_name" \
            -G Ninja \
            -DPHLEX_USE_FORM=ON \
            -DFORM_USE_ROOT_STORAGE=ON \
            -S "$SOURCE_ROOT" \
            -B "$BUILD_DIR" || {
            error "CMake configuration failed"
            exit 1
        }
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

    if ! cmake --build "$BUILD_DIR" --target coverage-symlink-clean; then
        warn "coverage-symlink-clean target unavailable or failed; .coverage-generated may remain."
    fi

    local artifact_dir="$WORKSPACE_ROOT/coverage-artifacts"
    if [[ -d "$artifact_dir" ]]; then
        log "Removing coverage artifact bundle: $artifact_dir"
        rm -rf "$artifact_dir"
    fi

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

        log "Normalizing coverage XML paths for tooling compatibility..."
        if ! cmake --build "$BUILD_DIR" --target coverage-xml-normalize; then
            error "Failed to normalize coverage XML. Adjust filters/excludes and retry."
            exit 1
        fi
    success "XML coverage report generated: $output_file"
    copy_gcov_artifacts_to_workspace
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
            if ! cmake --build "$BUILD_DIR" --target coverage-html-normalize; then
                error "Failed to normalize LCOV coverage report. Adjust filters/excludes and retry."
                exit 1
            fi
            copy_gcov_artifacts_to_workspace
        fi
    else
        warn "HTML coverage report not available (lcov dependency issues)"
    fi
}

copy_gcov_artifacts_to_workspace() {
    local xml_path="$BUILD_DIR/coverage.xml"
    if [[ -f "$xml_path" ]]; then
        cp "$xml_path" "$WORKSPACE_ROOT/coverage.xml"
        log "Coverage XML also available at: $WORKSPACE_ROOT/coverage.xml"
    fi

    local info_path="$BUILD_DIR/coverage.info.final"
    if [[ -f "$info_path" ]]; then
        log "Copying lcov.info to workspace root for VS Code Coverage Gutters..."
        cp "$info_path" "$WORKSPACE_ROOT/lcov.info"
        success "Coverage info file available at: $WORKSPACE_ROOT/lcov.info"
        cp "$info_path" "$WORKSPACE_ROOT/coverage.info"
    fi
}

generate_llvm_report() {
    if [[ "$COVERAGE_PRESET" != "coverage-clang" ]]; then
        error "LLVM coverage reports are only available with the coverage-clang preset"
        exit 1
    fi

    ensure_tests_current

    log "Generating LLVM coverage summary..."
    if ! cmake --build "$BUILD_DIR" --target coverage-llvm; then
        error "Failed to generate LLVM coverage summary"
        exit 1
    fi

    local summary_path="$BUILD_DIR/coverage-llvm.txt"
    if [[ -f "$summary_path" ]]; then
        success "LLVM coverage summary generated: $summary_path"
        if [[ -n "${WORKSPACE_ROOT:-}" ]]; then
            cp "$summary_path" "$WORKSPACE_ROOT/coverage-llvm.txt"
            log "Coverage summary also available at: $WORKSPACE_ROOT/coverage-llvm.txt"
        fi
    else
        error "Expected LLVM coverage summary not found at $summary_path"
        exit 1
    fi

    local lcov_path="$BUILD_DIR/coverage-llvm.info"
    if [[ -f "$lcov_path" ]]; then
        if [[ -n "${WORKSPACE_ROOT:-}" ]]; then
            cp "$lcov_path" "$WORKSPACE_ROOT/coverage-llvm.info"
            log "Coverage LCOV export also available at: $WORKSPACE_ROOT/coverage-llvm.info"
        fi
    else
        warn "Expected LLVM LCOV export not found at $lcov_path"
    fi
}

generate_llvm_html_report() {
    log "Generating LLVM HTML report..."
    # Generate LLVM coverage summary and .info export (also logs summary) before HTML report
    generate_llvm_report

    local lcov_path="$BUILD_DIR/coverage-llvm.info"
    if [[ ! -f "$lcov_path" ]]; then
        error "LLVM LCOV export not found at $lcov_path. Cannot generate HTML report."
        exit 1
    fi

    if ! command -v genhtml >/dev/null 2>&1; then
        error "'genhtml' command not found, which is required for HTML report generation."
        error "Please install lcov: 'sudo apt-get install lcov' or 'brew install lcov'"
        exit 1
    fi

    (cd "$BUILD_DIR" && genhtml -o coverage-html "$lcov_path" --title \
        "Phlex Coverage Report (Clang)" --show-details --legend --branch-coverage \
        --ignore-errors mismatch,inconsistent,negative,empty)

    if [[ -d "$BUILD_DIR/coverage-html" ]]; then
        success "HTML coverage report generated: $BUILD_DIR/coverage-html/"
    else
        error "Failed to generate HTML report from LLVM data."
        exit 1
    fi
}

show_summary() {
    ensure_tests_current
    check_build_dir
    log "Generating coverage summary via CMake target..."
    cmake --build "$BUILD_DIR" --target coverage-summary
}

view_html_internal() {
    log "Opening HTML coverage report..."
    if command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$BUILD_DIR/coverage-html/index.html"
    elif command -v open >/dev/null 2>&1; then
        open "$BUILD_DIR/coverage-html/index.html"
    else
        local report_path="$BUILD_DIR/coverage-html/index.html"
        local file_url=""
        if command -v python3 >/dev/null 2>&1; then
            file_url="$(python3 -c "import pathlib, sys; print(pathlib.Path(sys.argv[1]).resolve().as_uri())" "$report_path")"
        else
            file_url="file://$report_path"
        fi
        echo "HTML report available at: $file_url"
    fi
}

upload_codecov() {
    check_build_dir

    local coverage_file=""
    if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
        coverage_file="coverage-llvm.info"
        if [[ ! -f "$BUILD_DIR/$coverage_file" ]]; then
            warn "Clang LCOV report not found. Generate it first with '$0 report'"
            exit 1
        fi
    else
        coverage_file="coverage.xml"
        if [[ ! -f "$BUILD_DIR/$coverage_file" ]]; then
            warn "GCC XML report not found. Generate it first with '$0 xml'"
            exit 1
        fi
        log "Ensuring coverage XML paths are normalized before upload..."
        if ! cmake --build "$BUILD_DIR" --target coverage-xml-normalize; then
            error "Coverage XML failed normalization. Investigate filters/excludes before uploading."
            exit 1
        fi
        log "Coverage XML source roots after normalization:"
        grep -o '<source>.*</source>' "$BUILD_DIR/coverage.xml" | head -5 | sed 's/^/  /'
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
    if ! cmake --build "$BUILD_DIR" --target coverage-xml-normalize; then
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
    log "Coverage file: $BUILD_DIR/$coverage_file"

    # Build codecov command
    CODECOV_CMD=(codecov upload-coverage
                 --file "$coverage_file"
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
    if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
        generate_llvm_report
        generate_llvm_html_report
        success "Complete LLVM coverage analysis finished!"
        log "Summary report: $BUILD_DIR/coverage-llvm.txt"
        log "HTML report: $BUILD_DIR/coverage-html/index.html"
    else
        check_build_dir
        log "Generating GCC coverage report bundle..."
        if ! cmake --build "$BUILD_DIR" --target coverage-gcov; then
            error "Failed to generate GCC coverage reports during run_all"
            exit 1
        fi
        copy_gcov_artifacts_to_workspace

        success "Complete coverage analysis finished!"
        log "XML report: $BUILD_DIR/coverage.xml"
        if [[ -d "$BUILD_DIR/coverage-html" ]]; then
            log "HTML report: $BUILD_DIR/coverage-html/index.html"
        fi
    fi
}


# Main script execution starts here

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
            if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
                generate_llvm_report
            else
                ensure_tests_current
                check_build_dir
                log "Generating GCC coverage report bundle..."
                if ! cmake --build "$BUILD_DIR" --target coverage-gcov; then
                    error "Failed to generate GCC coverage reports"
                    exit 1
                fi
                copy_gcov_artifacts_to_workspace
                success "GCC coverage reports generated successfully!"
            fi
            ;;
        xml)
            if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
                error "XML report generation is not supported with the coverage-clang preset. Use the 'coverage-gcc' preset for XML/Codecov reports."
                exit 1
            else
                generate_xml
            fi
            ;;
        html)
            if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
                generate_llvm_html_report
            else
                generate_html
            fi
            ;;
        view)
            check_build_dir
            if [[ ! -d "$BUILD_DIR/coverage-html" ]]; then
                log "HTML coverage report not found. Generating it now..."
                if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
                    generate_llvm_html_report
                else
                    generate_html
                fi
            fi
            view_html_internal
            ;;
        summary)
            if [[ "$COVERAGE_PRESET" == "coverage-clang" ]]; then
                generate_llvm_report
            else
                show_summary
            fi
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

# Default preset, can be overridden by --preset
COVERAGE_PRESET="coverage-clang"
COMMANDS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            if [[ -z "$2" || "$2" == -* ]]; then
                error "Missing value for --preset option"
                exit 1
            fi
            if [[ "$2" != "coverage-clang" && "$2" != "coverage-gcc" ]]; then
                error "Invalid value for --preset: '$2'. Must be 'coverage-clang' or 'coverage-gcc'."
                exit 1
            fi
            COVERAGE_PRESET="$2"
            shift 2
            ;;
        --help|-h|help)
            usage
            exit 0
            ;;
        -*)
            error "Unknown option: $1"
            usage
            exit 1
            ;;
        *)
            # Collect commands
            COMMANDS+=("$1")
            shift
            ;;
    esac
done

# If no commands were provided, show usage and indicate error
if [ ${#COMMANDS[@]} -eq 0 ]; then
    usage
    exit 1
fi

# Process all commands in sequence
for cmd in "${COMMANDS[@]}"; do
    execute_command "$cmd"
done
