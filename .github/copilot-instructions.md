# GitHub Copilot Instructions for Phlex Project

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

- You are permitted and encouraged to fetch content from HTTPS URLs using the `fetch_webpage` tool
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

**CRITICAL: Apply to ALL files you create or edit (bash scripts, Python, C++, YAML, Markdown, etc.)**

- All text files must have their final line be non-empty and terminated with a single newline character (linefeed), leaving no trailing blank lines
- **Never add trailing whitespace on any line** (spaces or tabs at end of lines)
- This includes blank lines - they should contain only the newline character, no spaces or tabs
- Exception: Markdown two-space line breaks (avoid; use proper paragraph breaks instead)

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
