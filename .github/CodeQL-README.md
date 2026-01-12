```markdown
# CodeQL scanning for this repository

This repository uses C++ (C++20 / moving to C++23) built with CMake under the phlex-src directory, plus some Python and CI bits (Bash). The repository includes a CodeQL GitHub Actions workflow on branch `copilot/codeql-workflow` that:

- Runs on pushes to `main`, PRs targeting `main`, a weekly schedule, and can be run manually.
- Uses the repository's existing Phlex CMake build actions (not CodeQL autobuild) so the same build configuration is used for tests and release builds.
- Scans C++ and Python sources and is scoped to the `phlex-src` tree (see the CodeQL config).
- Uses RelWithDebInfo build type in CI so debug symbols are present while keeping realistic optimization.

Important workflow-specific notes
- The workflow sets `autobuild: false` during the CodeQL init so the repository's own configure / build steps run. This is intentional: the Phlex build actions are used to build exactly what you ship.
- The workflow tries to locate and copy a compile_commands.json (from `phlex-src/build/` or `phlex-build/`) to the workspace root so diagnostic tools and manual inspection have a predictable path.
- The workflow runs inside the repository container image (if provided) so it uses the same toolchain and environment your CI expects.

How the workflow is targeted at phlex-src
- The CMake configure & build steps are run with `-S phlex-src -B phlex-src/build` (or your Phlex-specific helpers are invoked in that context) so compile units, compiler flags, and generated compile_commands reference files under `phlex-src`.
- The CodeQL config (`.github/codeql/codeql-config.yml`) contains an explicit `paths.include: - phlex-src/**` entry and excludes common vendor/build directories. This ensures CodeQL focuses on the intended code and not third-party or generated artifacts.

Recommended build type for CodeQL runs
- Use RelWithDebInfo (the workflow is already set to use this). Rationale:
  - RelWithDebInfo produces debug symbols (required for better mapping of findings to source/stack traces) while compiling with optimizations closer to production.
  - Pure Debug (-O0 -g) can be used for local triage of tricky alerts but is slower and sometimes produces analysis results that differ from optimized builds.
  - Release (no debug info) is not recommended for CodeQL because missing debug symbols reduce the quality of evidence and traces in findings.

How to run CodeQL locally (examples)
1. Install the CodeQL CLI: https://codeql.github.com/docs/codeql-cli/getting-started/
2. Create a C++ database for the phlex-src tree (example):
   - From repository root:
     codeql database create codeql-db --language=cpp --command="cmake -S phlex-src -B phlex-src/build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build phlex-src/build -- -j$(nproc)"
3. Analyze the database:
   - codeql database analyze codeql-db --format=sarifv2 --output=results-cpp.sarif github/codeql/cpp-security-and-quality
4. Python example (if you need to build a Python DB):
   - codeql database create codeql-python-db --language=python --command="python3 -m pip install -r phlex-src/requirements.txt" 
   - codeql database analyze codeql-python-db --format=sarifv2 --output=results-py.sarif github/codeql/python-security-and-quality
5. Open the SARIF in VS Code via the CodeQL extension or upload results via the GitHub UI.

Triage tips and workflows
- Start with high-confidence, high-severity alerts.
- Use the evidence shown in the CodeQL alert to locate the vulnerable trace. RelWithDebInfo gives better evidence than Release.
- When marking false positives, add a short rationale in the Code Scanning UI (this helps future auditors).
- If the repo has many historical findings, consider a triage sprint to baseline or create a SARIF baseline to ignore existing alerts temporarily while blocking new ones.

How to add or change query packs
- The repository currently selects the language security-and-quality packs for C++ and Python. That is a good default.
- If you want to add additional packs (experimental or specialized) you can:
  - Edit `.github/codeql/codeql-config.yml` and add additional packs there (use the canonical pack name), or
  - Add them to the init step in the workflow (`with: queries:`).
- Example of adding packs in the workflow init:
  ```yaml
  uses: github/codeql-action/init@v4
  with:
    languages: cpp, python
    queries: |
      github/codeql/cpp-security-and-quality
      github/codeql/python-security-and-quality
      # additional packs...
    autobuild: false
  ```

Action-specific / workflow scanning
- There are CodeQL query packs that specifically analyze GitHub Actions workflow files (YAML) to find insecure patterns (for example: unsafe use of secrets, untrusted inputs in workflow run steps, usage of unpinned actions, etc.). If you rely on custom workflows or pass secrets/inputs to actions, consider enabling the GitHub Actions query pack if it is available in your CodeQL pack index.
- To discover whether an official GitHub Actions pack exists or to find its exact name, see the CodeQL packs index (the canonical source) or search the public CodeQL repository:
  - https://github.com/github/codeql/tree/main/packs
  - Search for "github-actions" or "actions" in the CodeQL repo to find action-related packs and their exact pack names.

Recommended query packs for this repository (starting point)
- github/codeql/cpp-security-and-quality
  - Purpose: Core security and quality queries for C and C++ codebases. Good coverage of common memory safety, API misuse, and typical C/C++ pitfalls.
  - Why: Phlex is primarily C++, so this pack is the most important starting point.
- github/codeql/python-security-and-quality
  - Purpose: Core security and quality queries for Python code. Scans for common Python-specific vulnerabilities and quality issues.
  - Why: Phlex contains some Python; include this pack so those files are analyzed.

Other useful pack categories to consider
- Language experimental packs (e.g., "cpp-experimental" / "python-experimental")
  - These provide newer/experimental queries that are not yet in the stable security-and-quality pack. Use cautiously: they can produce more findings and more false positives.
- Action/workflow packs (GitHub Actions specific)
  - Inspect action/workflow packs if you want automated scanning of your workflow YAML files for insecure patterns (untrusted inputs, leaking secrets, unpinned/unsigned actions).
- Query packs for third-party libraries / frameworks (if applicable)
  - If Phlex heavily uses a certain third-party library that has its own pack, consider enabling it to get library-specific rules.

How to find the exact pack names and descriptions
- Official source: the GitHub CodeQL packs and QL repos:
  - https://github.com/github/codeql/tree/main/packs
  - https://github.com/github/codeql/tree/main/ql
- CodeQL CLI (partial support for pack discovery):
  - You can use `codeql pack` and `codeql resolve` commands to inspect installed packs. The CodeQL CLI documentation shows usage for installing and listing packs.
- GitHub docs:
  - Code scanning docs and the CodeQL repo README often list recommended packs and query categories.

Suggested immediate next steps
1. Keep the current packs (cpp-security-and-quality and python-security-and-quality) enabled in `.github/codeql/codeql-config.yml` — they are the right baseline.
2. Search the CodeQL packs index for any GitHub Actions pack and enable it if you want workflow-level checks.
3. If you want more thorough coverage, enable the experimental packs temporarily in a non-blocking run, review the alerts, then decide whether to include them in CI permanently.
4. If you want, I can run a short search and report back exact pack names for GitHub Actions and experimental packs — tell me if you want me to search the public CodeQL repo for matching pack names and list them with links.

## Viewing CodeQL Alerts

### For Pull Requests from the Same Repository

When a PR is opened from a branch in the same repository (not a fork), the CodeQL workflow will automatically post a comment on the PR with details about any new or resolved alerts.

### For Pull Requests from Forks

When a PR is opened from a fork, the `GITHUB_TOKEN` does not have permission to post comments on the PR due to GitHub's security model. In this case:

1. **The workflow will display alert information in the job logs**: Look for the "Log CodeQL alert changes for forked PR" step in the workflow run. The alert details will be shown there with `::error::` or `::notice::` prefixes.

2. **Download the debug log artifact**: The workflow uploads a `codeql-alerts-debug-log` artifact containing detailed information about the alerts. You can download this from the workflow run's artifacts section.

3. **Check the Security tab**: CodeQL alerts from fork PRs are still uploaded to the Code Scanning alerts in the base repository. Repository maintainers with appropriate permissions can view these alerts by:
   - Navigating to the repository's Security tab
   - Clicking on "Code scanning alerts"
   - Filtering by branch or PR number to find alerts specific to the fork PR

4. **Ask a maintainer**: If you're the fork PR author and need details about the alerts, you can ask a repository maintainer to review the Code Scanning alerts for your PR.

### Understanding the Workflow Behavior

- **New alerts detected**: The workflow will fail to prevent merging code with new security issues.
- **Fixed alerts only**: The workflow will succeed and provide informational notices.
- **No permission to comment**: This is expected GitHub behavior for fork PRs and is not a bug.

## Contact / Ownership
- Consider adding a CODEOWNERS file for the phlex-src tree so triage notifications reach the most appropriate maintainers.
```
