"""Security regression tests for the formatting workflow's publishing boundary."""

from __future__ import annotations

import json
import runpy
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

from prepare_formatting_commit import prepare_commit

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/pre-commit-fix.yaml"


def git(repository, *args):
    """Run Git in a disposable test repository."""
    return subprocess.check_output(["git", "-C", str(repository), *args])


@pytest.fixture
def repository(tmp_path):
    """Create a committed regular file and a clean publishing checkout."""
    repo = tmp_path / "repo"
    repo.mkdir()
    git(repo, "init")
    (repo / "file.py").write_text("x = 1\n")
    (repo / "executable.sh").write_text("#!/bin/sh\nexit 0\n")
    (repo / "executable.sh").chmod(0o755)
    git(repo, "add", ".")
    git(repo, "-c", "user.name=Test", "-c", "user.email=test@example.com", "commit", "-m", "base")
    return repo, git(repo, "rev-parse", "HEAD").decode().strip()


def export_patch(repository, tmp_path):
    """Export staged changes and restore a clean publishing index and worktree."""
    patch = tmp_path / "formatting.patch"
    patch.write_bytes(git(repository, "diff", "--cached", "--binary", "--no-renames"))
    git(repository, "restore", "--staged", "--worktree", ".")
    # New files restored out of the index remain untracked.
    git(repository, "clean", "-fd")
    return patch


@pytest.mark.parametrize("filename", ["file.py", "executable.sh", "space name.py"])
def test_creates_commit_without_executing_code(repository, tmp_path, filename):
    """Content changes preserve modes and never invoke repository hooks or changed scripts."""
    repo, sha = repository
    if filename == "space name.py":
        git(repo, "mv", "file.py", filename)
        git(
            repo,
            "-c",
            "user.name=Test",
            "-c",
            "user.email=test@example.com",
            "commit",
            "-m",
            "rename",
        )
        sha = git(repo, "rev-parse", "HEAD").decode().strip()
    marker = tmp_path / "executed"
    hook = repo / ".git/hooks/pre-commit"
    hook.write_text(f"#!/bin/sh\ntouch '{marker}'\nexit 1\n")
    hook.chmod(0o755)
    content = f"#!/bin/sh\ntouch '{marker}'\n"
    (repo / filename).write_text(content)
    git(repo, "add", filename)
    patch = export_patch(repo, tmp_path)
    commit = prepare_commit(repo, patch, sha)
    assert git(repo, "show", f"{commit}:{filename}").decode() == content
    assert git(repo, "rev-parse", f"{commit}^").decode().strip() == sha
    assert git(repo, "rev-parse", "HEAD").decode().strip() == sha
    assert not marker.exists()


@pytest.mark.parametrize("change", ["addition", "deletion", "mode", "symlink", "submodule"])
def test_rejects_non_content_changes(repository, tmp_path, change):
    """Untrusted patches cannot add/delete files or introduce executable plumbing."""
    repo, sha = repository
    file = repo / "file.py"
    if change == "addition":
        (repo / "new.py").write_text("x = 2\n")
        git(repo, "add", "new.py")
    elif change == "deletion":
        git(repo, "rm", "file.py")
    elif change == "mode":
        git(repo, "update-index", "--chmod=+x", "file.py")
    elif change == "symlink":
        file.unlink()
        file.symlink_to("../outside")
        git(repo, "add", "file.py")
    else:
        git(repo, "update-index", "--add", "--cacheinfo", f"160000,{sha},module")
    patch = export_patch(repo, tmp_path)
    with pytest.raises(ValueError):
        prepare_commit(repo, patch, sha)
    assert git(repo, "rev-parse", "HEAD").decode().strip() == sha


def test_empty_patch(repository, tmp_path):
    """No formatting changes produce no commit."""
    repo, sha = repository
    patch = tmp_path / "empty.patch"
    patch.write_bytes(b"")
    assert prepare_commit(repo, patch, sha) == ""


def test_entrypoint_prints_commit(repository, tmp_path, monkeypatch, capsys):
    """The publishing command prints the created commit SHA and preserves HEAD."""
    repo, sha = repository
    content = "x = 2\n"
    (repo / "file.py").write_text(content)
    git(repo, "add", "file.py")
    patch = export_patch(repo, tmp_path)
    script = Path(__file__).resolve().parents[1] / "prepare_formatting_commit.py"
    monkeypatch.setattr(sys, "argv", [str(script), str(repo), str(patch), sha])

    runpy.run_path(str(script), run_name="__main__")

    output = capsys.readouterr()
    commit = output.out.strip()
    assert len(commit) == 40
    assert output.out == f"{commit}\n"
    assert output.err == ""
    assert git(repo, "show", f"{commit}:file.py").decode() == content
    assert git(repo, "rev-parse", f"{commit}^").decode().strip() == sha
    assert git(repo, "rev-parse", "HEAD").decode().strip() == sha


def test_entrypoint_rejects_invalid_sha(repository, tmp_path, monkeypatch, capsys):
    """The publishing command reports malformed SHAs on stderr and exits with failure."""
    repo, sha = repository
    patch = tmp_path / "empty.patch"
    patch.write_bytes(b"")
    script = Path(__file__).resolve().parents[1] / "prepare_formatting_commit.py"
    monkeypatch.setattr(sys, "argv", [str(script), str(repo), str(patch), "not-a-sha"])

    with pytest.raises(SystemExit) as error:
        runpy.run_path(str(script), run_name="__main__")

    assert error.value.code == 1
    output = capsys.readouterr()
    assert output.out == ""
    assert output.err == "Formatting patch rejected: Invalid target SHA\n"
    assert git(repo, "rev-parse", "HEAD").decode().strip() == sha
    assert git(repo, "status", "--porcelain") == b""


def test_binary_patch(repository, tmp_path):
    """Binary content updates are applied as data without checkout filters."""
    repo, sha = repository
    content = b"\x00formatted\xff\n"
    (repo / "file.py").write_bytes(content)
    git(repo, "add", "file.py")
    patch = export_patch(repo, tmp_path)
    commit = prepare_commit(repo, patch, sha)
    assert git(repo, "show", f"{commit}:file.py") == content


@pytest.mark.parametrize("kind", ["sha", "dirty", "symlink", "oversized", "traversal", "git"])
def test_rejects_invalid_inputs(repository, tmp_path, kind):
    """Invalid revisions, artifacts, checkouts, and unsafe paths fail closed."""
    repo, sha = repository
    patch = tmp_path / "formatting.patch"
    patch.write_bytes(b"")
    if kind == "sha":
        sha = "0" * 40
    elif kind == "dirty":
        (repo / "file.py").write_text("changed\n")
    elif kind == "symlink":
        patch.unlink()
        patch.symlink_to(repo / "file.py")
    elif kind == "oversized":
        with patch.open("wb") as stream:
            stream.truncate(10 * 1024 * 1024 + 1)
    else:
        path = "../outside" if kind == "traversal" else ".git/config"
        patch.write_text(
            f"diff --git a/{path} b/{path}\n--- a/{path}\n+++ b/{path}\n@@ -1 +1 @@\n-old\n+new\n"
        )
    with pytest.raises((ValueError, subprocess.CalledProcessError)):
        prepare_commit(repo, patch, sha)


def test_workflow_privilege_boundary():
    """Only trusted publishing steps receive credentials; target execution is SHA-pinned."""
    workflow = yaml.safe_load(WORKFLOW.read_text())
    assert workflow["on"] == {"issue_comment": {"types": ["created"]}}
    assert workflow["permissions"] == {"contents": "read"}
    jobs = workflow["jobs"]
    formatter = jobs["apply_fixes"]
    assert formatter["permissions"] == {"contents": "read"}
    checkout = formatter["steps"][0]["with"]
    assert checkout["ref"] == "${{ needs.setup.outputs.sha }}"
    assert checkout["persist-credentials"] is False
    assert "secrets." not in str(formatter)
    publisher = jobs["publish"]
    assert "./.github/actions" not in str(publisher)
    assert "pre_commit" not in str(publisher)
    push = next(step for step in publisher["steps"] if step["name"] == "Push fixes")
    assert push["env"]["PUSH_TOKEN"] == "${{ secrets.WORKFLOW_PAT }}"
    assert '--force-with-lease="refs/heads/$TARGET_REF:$TARGET_SHA"' in push["run"]
    assert "--force " not in push["run"]
    assert 'test "${current%%$\'\\t\'*}" = "$TARGET_SHA"' in push["run"]
    stale = next(step for step in publisher["steps"] if step["name"] == "Reject stale target")
    assert "pr.head.sha !== sha" in stale["with"]["script"]
    assert "pr.head.repo?.full_name !== repo" in stale["with"]["script"]
    trusted_checkout = publisher["steps"][0]["with"]
    assert trusted_checkout["ref"] == "${{ needs.setup.outputs.trusted_sha }}"
    assert "github.event.issue.pull_request" in jobs["setup"]["if"]
    assert "github.event.comment.author_association" in jobs["setup"]["if"]
    resolver = jobs["setup"]["steps"][0]["with"]["script"]
    assert "branch: context.payload.repository.default_branch" in resolver
    assert "core.setOutput('trusted_sha', trustedBranch.commit.sha)" in resolver


@pytest.mark.parametrize("change", [None, "delete", "rewind", "advance"])
def test_atomic_push_lease(repository, tmp_path, change):
    """The exact-SHA push lease rejects all target changes, including ref deletion."""
    repo, parent = repository
    (repo / "file.py").write_text("x = 2\n")
    git(repo, "add", "file.py")
    git(repo, "-c", "user.name=Test", "-c", "user.email=test@example.com", "commit", "-m", "head")
    sha = git(repo, "rev-parse", "HEAD").decode().strip()
    remote = tmp_path / "remote.git"
    subprocess.run(["git", "init", "--bare", str(remote)], check=True, capture_output=True)
    git(repo, "remote", "add", "origin", str(remote))
    git(repo, "push", "origin", f"{sha}:refs/heads/topic")
    (repo / "file.py").write_text("x = 3\n")
    git(repo, "add", "file.py")
    commit = prepare_commit(repo, export_patch(repo, tmp_path), sha)
    if change == "delete":
        git(remote, "update-ref", "-d", "refs/heads/topic")
    elif change == "rewind":
        git(remote, "update-ref", "refs/heads/topic", parent)
    elif change == "advance":
        tree = git(repo, "rev-parse", f"{sha}^{{tree}}").decode().strip()
        advanced = (
            git(
                repo,
                "-c",
                "user.name=Test",
                "-c",
                "user.email=test@example.com",
                "commit-tree",
                tree,
                "-p",
                sha,
                "-m",
                "concurrent update",
            )
            .decode()
            .strip()
        )
        git(repo, "push", "origin", f"{advanced}:refs/heads/topic")
    # The changed branch above models an update after the workflow's ls-remote check.
    result = subprocess.run(
        [
            "git",
            "-C",
            str(repo),
            "push",
            f"--force-with-lease=refs/heads/topic:{sha}",
            "origin",
            f"{commit}:refs/heads/topic",
        ],
        capture_output=True,
        text=True,
    )
    assert (result.returncode == 0) == (change is None), result.stderr


@pytest.mark.skipif(shutil.which("node") is None, reason="Node.js is required for API mocks")
@pytest.mark.parametrize("change", [None, "sha", "repo", "ref", "closed"])
def test_stale_target_api_guard(change):
    """Execute the actual workflow guard against mocked GitHub responses."""
    jobs = yaml.safe_load(WORKFLOW.read_text())["jobs"]
    step = next(step for step in jobs["publish"]["steps"] if step["name"] == "Reject stale target")
    pr = {
        "state": "open",
        "head": {"repo": {"full_name": "owner/repo"}, "ref": "topic", "sha": "a" * 40},
    }
    if change == "closed":
        pr["state"] = "closed"
    elif change == "repo":
        pr["head"]["repo"]["full_name"] = "other/repo"
    elif change:
        pr["head"][change] = "changed"
    harness = f"""
      const context = {{repo: {{}}, issue: {{number: 440}}}};
      const pr = {json.dumps(pr)};
      process.env.TARGET_REPO = 'owner/repo';
      process.env.TARGET_REF = 'topic';
      process.env.TARGET_SHA = '{"a" * 40}';
      const github = {{rest: {{pulls: {{get: async () => ({{data: pr}})}}}}}};
      (async () => {{ {step["with"]["script"]} }})().catch(() => process.exit(1));
    """
    result = subprocess.run(["node", "-e", harness], capture_output=True, text=True)
    assert result.returncode == (0 if change is None else 1), result.stderr
