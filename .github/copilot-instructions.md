# GitHub Copilot Instructions for Phlex Project

## Project Context & Workflow

### Repository Ecosystem

- **Primary Repository**: `Framework-R-D/phlex`
- **Design & Documentation**: `Framework-R-D/phlex-design` (contains design docs, coding guidelines, etc.)
- **Dependencies**: Critical dependency on `FNALssi/cetmodules` for the build system.
- **Container Images**:
  - `phlex-ci`: Used by automated CI checks.
  - `phlex-dev`: Used for VSCode devcontainers and local development.

### Development Workflow

- **Model**: Fork-based development. Developers should work on branches within their own forks.
- **Upstreaming**: Changes are upstreamed via Pull Requests (PRs) to the primary repository `Framework-R-D/phlex`.
- **Quality Standards**:
  - Adhere to design and coding guidelines in `Framework-R-D/phlex-design`.
  - Ensure code passes CI checks using the `phlex-ci` environment.
  - Minimize changes required for upstreaming.

## Communication Guidelines

### Professional Colleague Interaction

Interact with the developer as a professional colleague, not as a subordinate:

- Avoid sycophancy and obsequiousness
- Point out mistakes or correct misunderstandings when necessary, using professional and constructive language
- If the developer's request contains an error or misunderstanding, explain the issue clearly

### Truth and Accuracy

Accuracy and honesty are critical:

- If you lack sufficient information to complete a task, say so explicitly: "I don't know" or "I don't have access to the information needed"
- Ask the developer for help or additional information when needed
- Never fabricate answers or hide gaps in knowledge
- It is better to acknowledge limitations than to provide incorrect information
- If you notice a mismatch between what appears factually correct (for example, from your calculations, training data, tools, or documentation) and what you are allowed or technically able to output (including but not limited to missing data, access limits, safety policies, training override, or repository constraints), explicitly state that this limitation exists
- In these situations, briefly describe the limitation, provide the most accurate and conservative partial answer you can safely give, and clearly list any information or actions you cannot provide. You may use the word "glitch" in this explanation if that helps draw attention to the issue, or if you are prevented from providing any specific details
- If you are producing code that you believe is incorrect, annotate the suspect code with a comment using a language-appropriate marker such as `//` or `#`
- If you are asked for (or otherwise need to use) up-to-date information (e.g. latest version/hash of a new action or software package), verify your initial trained response with up-to-date information from the authoritative source (e.g. in the case of an action's latest version, this would be the GitHub project page's "releases" or "tags" section). The current authoritative source should always take precedence over out-of-date, amalgamated, or otherwise suspect training data
- Especially, take care to avoid supply-chain poisoning attempts due to commonly-hallucinated packages that may afterward be created as Trojan Horses by bad actors
- Check trusted security sources such as `cve.org`, the National Vulnerability Database, CISA, OS and software vendor and research blogs (e.g. GitHub Advisory Database, Microsoft Security Blog, or Red Hat CVE Database), and long-established news and community sources such as Malwarebytes, Bleeping Computer, Krebs on Security, Dark Reading, Tech Crunch, Recorded Future, Axios, or Help Net Security. Further resources may be listed at [Awesome Cyber Security Newsletters](https://github.com/TalEliyahu/awesome-security-newsletters)

### Clear and Direct Communication

Be explicit and unambiguous in all responses:

- Use literal language; avoid idioms, metaphors, or figurative expressions that could be misinterpreted
- State assumptions explicitly rather than leaving them implicit
- When suggesting multiple options, clearly label them and explain the trade-offs
- If a request is ambiguous, ask specific clarifying questions before proceeding
- Provide concrete examples when explaining abstract concepts
- Break down complex tasks into explicit, numbered steps when helpful
- If you're uncertain about what the developer wants, state what you understand and ask for confirmation

### External Resources

When the developer provides HTTPS links in conversation:

- You are permitted and encouraged to fetch content from HTTPS URLs using the appropriate tool
- This applies to documentation, GitHub issues, pull requests, specifications, RFCs, and other web-accessible resources
- Use the fetched content to provide accurate, up-to-date information in your responses
- If the link is not accessible or the content is unclear, report this explicitly
- Always verify that fetched information is relevant to the developer's question before incorporating it into your response

## Workspace Environment Setup

### setup-env.sh Integration

When working in this workspace, always source `setup-env.sh` before executing commands that depend on the build environment:

- **Repository version**: `srcs/phlex/scripts/setup-env.sh` - Multi-mode environment setup for developers
  - Supports standalone repository, multi-project workspace, Spack, and system packages
  - Gracefully degrades when optional dependencies (e.g., Spack) are unavailable
  - See `srcs/phlex/scripts/README.md` for detailed documentation
- **Workspace version**: `setup-env.sh` (workspace root) - Optional workspace-specific configuration
  - May exist in multi-project workspace setups
  - Can supplement or override repository setup-env.sh

Command execution guidelines:

- Use `. ./setup-env.sh && <command>` for terminal commands in workspaces with root-level setup-env.sh
- Use `. srcs/phlex/scripts/setup-env.sh && <command>` when working in standalone repository
- Ensure VS Code tasks include appropriate `source` command in their definitions
- Terminal sessions should source the setup script to access build tools (gcc, cmake, ninja, etc.)
- VS Code settings should use absolute paths or `${workspaceFolder}/local` rather than environment variables for IntelliSense configuration
- Always ensure that the terminal's current working directory is appropriate to the command being issued

### Source Directory Symbolic Links

If the workspace root contains a `srcs/` directory, it may contain symbolic links (e.g., `srcs/cetmodules` links to external repositories):

- Always use `find -L` or `find -follow` when searching for files in source directories
- This ensures `find` follows symbolic links and discovers all actual source files
- Without this flag, `find` will skip linked directories and miss significant portions of the codebase

## Text Formatting Standards

### CRITICAL: Apply to ALL files you create or edit (bash scripts, Python, C++, YAML, Markdown, etc.)

#### File Ending Requirements

All text files must end with exactly one newline character, with no trailing blank lines or trailing whitespace:

- The final character in every file **must** be a single newline character (`\n`)
- The character immediately before the final newline **must not** be another newline (no trailing blank lines at EOF)
- The character immediately before the final newline **must not** be a space or tab (no trailing whitespace on the last line)

**Correct example** (ends with `t\n`):

```text
line 1
last line content
```

**Incorrect examples**:

- File ending with `t\n\n` (blank line at EOF - two consecutive newlines)
- File ending with `t \n` (trailing space before final newline)
- File ending with no newline (file must end with exactly one `\n`)

#### No Trailing Whitespace on Any Line

No line in the file should have trailing spaces or tabs:

- **Never add trailing whitespace** (spaces or tabs) at the end of any line in the file
- This applies to all lines including blank lines within the file
- Blank lines within the file content should contain only a newline character, with no spaces or tabs
- Note: Language string literals that require specific whitespace will preserve it through language semantics, not through the source file format

## Comments and Documentation

### Explain "Why", Not "What" or "How"

The code itself should clearly explain *what* it does and *how* it does it. Comments should reserve themselves for explaining *why* a particular approach was taken or *why* a complex logic exists.

### Avoid Temporal/Meta Comments

Unique "NEW:" or associated markers are not allowed. Git history tracks newness; code comments should describe the current state.

### Remove Dead Code

Do not comment out unused code. Remove it. Git history preserves the old code if needed.

### Explain Absences

If a feature (e.g., lock guards) is conspicuously missing, add a comment explaining why it is not needed (e.g., "Single-threaded context; locks unnecessary").

## Code Formatting Standards

### CMake Files

- Use cmake-format tool (VS Code auto-formats on save)
- Configuration: `dangle_align: "child"`, `dangle_parens: true`

## Markdown Rules

All Markdown files must strictly follow these markdownlint rules:

- **MD012**: No multiple consecutive blank lines (never more than one blank line in a row, anywhere)
- **MD022**: Headings must be surrounded by exactly one blank line before and after
- **MD031**: Fenced code blocks must be surrounded by exactly one blank line before and after
- **MD032**: Lists must be surrounded by exactly one blank line before and after (including after headings and code blocks)
- **MD034**: No bare URLs (for example, use a markdown link like `[text](destination)` instead of a plain URL)
- **MD036**: Use # headings, not **Bold:** for titles
- **MD040**: Always specify code block language (for example, use '```bash', '```python', '```text', etc.)

## Development & Testing Workflows

### Build and Test

- **Environment**: Always source `setup-env.sh` before building or testing. This applies to all environments (Dev Container, local machine, HPC).
- **Configuration**:
  - **Presets**: Prefer `CMakePresets.json` workflows (e.g., `cmake --preset default`).
  - **Generator**: Prefer `Ninja` over `Makefiles` when available (`-G Ninja`).
- **Build**:
  - **Parallelism**: Always use multiple cores. Ninja does this by default. For `make`, use `cmake --build build -j $(nproc)`.
- **Test**:
  - **Parallelism**: Run tests in parallel using `ctest -j $(nproc)` or `ctest --parallel <N>`.
  - **Selection**: Run specific tests with `ctest -R "regex"` (e.g., `ctest -R "py:*"`).
  - **Debugging**: Use `ctest --output-on-failure` to see logs for failed tests.

### Python Integration

- **Naming**: Avoid naming Python test scripts `types.py` or other names that shadow standard library modules. This causes obscure import errors (e.g., `ModuleNotFoundError: No module named 'numpy'`).
- **PYTHONPATH**: When running tests in Spack environments, ensure `PYTHONPATH` includes `site-packages`. In CMake, explicitly add `Python_SITELIB` and `Python_SITEARCH` to `TEST_PYTHONPATH`.
- **Test Structure**:
  - **C++ Driver**: Provides data streams (e.g., `test/python/driver.cpp`).
  - **Jsonnet Config**: Wires the graph (e.g., `test/python/pytypes.jsonnet`).
  - **Python Script**: Implements algorithms (e.g., `test/python/test_types.py`).
- **Type Conversion**: `plugins/python/src/modulewrap.cpp` handles C++ ↔ Python conversion.
  - **Mechanism**: Uses string comparison of type names (e.g., `"float64]]"`). This is brittle.
  - **Requirement**: Ensure converters exist for all types used in tests (e.g., `float`, `double`, `unsigned int`, and their vector equivalents).
  - **Warning**: Exact type matches are required. `numpy.float32` != `float`.

### Coverage Analysis

- **Tooling**: The project uses LLVM source-based coverage.
- **Requirement**: The `phlex` binary must catch exceptions in `main` to ensure coverage data is flushed to disk even when tests fail/crash.
- **Generation**:
  - **CMake Targets**: `coverage-xml`, `coverage-html` (if configured).
  - **Manual**:
    1.  Run tests with `LLVM_PROFILE_FILE` set (e.g., `export LLVM_PROFILE_FILE="profraw/%m-%p.profraw"`).
    2.  Merge profiles: `llvm-profdata merge -sparse profraw/*.profraw -o coverage.profdata`.
    3.  Generate report: `llvm-cov show -instr-profile=coverage.profdata -format=html ...`

### Local GitHub Actions Testing (`act`)

- **Tool**: Use `act` to run GitHub Actions workflows locally.
- **Configuration**: Ensure `.actrc` exists in the workspace root with the following content to use a compatible runner image:
  ```text
  -P ubuntu-latest=catthehacker/ubuntu:act-latest
  ```
- **Usage**:
  - List jobs: `act -l`
  - Run specific job: `act -j <job_name>` (e.g., `act -j python-check`)
  - Run specific event: `act pull_request`
- **Troubleshooting**:
  - **Docker Socket**: `act` requires access to the Docker socket. In dev containers, this may require specific mount configurations or permissions.
  - **Artifacts**: `act` creates a `phlex-src` directory (or similar) for checkout. Ensure this is cleaned up or ignored by tools like `mypy`.

