```markdown
# CodeQL scanning for this repository

This repository uses C++ (C++20/23 via CMake) and some Python. The included GitHub Actions workflow runs CodeQL analysis on pushes to `main`, on pull requests into `main`, and weekly.

Key points:
- The workflow explicitly configures and builds the project with CMake so CodeQL can generate a proper C/C++ database.
- Python scanning is included; ensure any Python dependencies are listed in requirements.txt if analysis needs them.
- The CodeQL config file (.github/codeql/codeql-config.yml) selects the C++ and Python query packs and excludes common generated, vendor, or build directories.

Running locally with CodeQL CLI
1. Install the CodeQL CLI: https://codeql.github.com/docs/codeql-cli/getting-started/
2. Create a database for C++ (example):
   - codeql database create codeql-db --language=cpp --command="cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -- -j$(nproc)"
3. Analyze the database:
   - codeql database analyze codeql-db --format=sarifv2 --output=results.sarif github/codeql/cpp-security-and-quality
4. Open the SARIF in VS Code with the CodeQL extension or upload via GitHub UI.

Triage tips
- Start with high-confidence/high-severity results.
- For false positives, add notes in the Code Scanning UI and consider repository-specific suppressions only when justified.
- If many historical alerts exist, consider creating an initial baseline or triage sprint to clear noise before requiring checks as blocking.

Customization notes
- If your CMake build needs special flags, toolchains, or external dependencies, update the `Configure & build (CMake)` step in the workflow accordingly.
- For cross-compilation or complex toolchains (e.g., embedded targets), you may need to create CodeQL databases locally and upload SARIFs instead of relying on the Actions runner to build.

Contact / Ownership
- Add CODEOWNERS or a small triage team to maintain and respond to CodeQL alerts.

```