"""Tests for sarif-alerts.py.

The script is a user-callable CLI tool that reads one or more SARIF files (or
directories of SARIF files) and prints a human-readable summary.  Tests cover:

- _collect_sarif_paths: file vs directory expansion
- _process_sarif: level/baseline filtering, location extraction, message
  truncation, error handling
- main(): argument parsing, missing files, directory input, exit codes
"""

from __future__ import annotations

import importlib.util
import json
import os
from collections.abc import Callable, Iterator
from pathlib import Path
from typing import Any

import pytest

# ---------------------------------------------------------------------------
# Import the module.  The file is named "sarif-alerts.py" (with a hyphen),
# which is not a valid Python identifier, so importlib is required.
# sys.path is set up by scripts/test/conftest.py.
# ---------------------------------------------------------------------------
_spec = importlib.util.spec_from_file_location(
    "sarif_alerts", Path(__file__).parent.parent / "sarif-alerts.py"
)
assert _spec is not None and _spec.loader is not None
sarif_alerts = importlib.util.module_from_spec(_spec)
# exec_module is defined on ExecutionLoader (a subclass of Loader); the assert
# above confirms loader is not None, and spec_from_file_location always returns
# a SourceFileLoader which does implement exec_module.
assert hasattr(_spec.loader, "exec_module")
_spec.loader.exec_module(sarif_alerts)  # type: ignore[union-attr]

# Typed aliases so Pylance can reason about call sites instead of treating
# these as Unknown (the module was loaded dynamically via importlib).
# pylint: disable=protected-access
# pylint: disable-next=line-too-long
_collect: Callable[[list[Path]], list[Path]] = sarif_alerts._collect_sarif_paths  # type: ignore[attr-defined]
_process: Callable[..., Iterator[str]] = sarif_alerts._process_sarif  # type: ignore[attr-defined]
# pylint: enable=protected-access
_main: Callable[[list[str] | None], int] = sarif_alerts.main  # type: ignore[attr-defined]

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_result(
    rule_id: str = "py/sql-injection",
    level: str = "error",
    baseline_state: str = "new",
    uri: str = "src/app.py",
    start_line: int | None = 10,
    message: str = "Possible SQL injection.",
) -> dict[str, Any]:
    region: dict[str, Any] = {}
    if start_line is not None:
        region["startLine"] = start_line
    return {
        "ruleId": rule_id,
        "level": level,
        "baselineState": baseline_state,
        "message": {"text": message},
        "locations": [
            {
                "physicalLocation": {
                    "artifactLocation": {"uri": uri},
                    "region": region,
                }
            }
        ],
    }


def _make_sarif(results: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "version": "2.1.0",
        "runs": [{"tool": {"driver": {"name": "CodeQL"}}, "results": results}],
    }


def _write_sarif(path: Path, results: list[dict[str, Any]]) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(_make_sarif(results)), encoding="utf-8")
    return path


# ---------------------------------------------------------------------------
# _collect_sarif_paths
# ---------------------------------------------------------------------------


class TestCollectSarifPaths:
    """Tests for TestCollectSarifPaths."""

    def test_file_returned_as_is(self, tmp_path: Path) -> None:
        """File returned as is."""
        f = tmp_path / "a.sarif"
        f.write_text("{}", encoding="utf-8")
        result = _collect([f])
        assert result == [f]

    def test_directory_expanded_to_sarif_files(self, tmp_path: Path) -> None:
        """Directory expanded to sarif files."""
        (tmp_path / "a.sarif").write_text("{}", encoding="utf-8")
        (tmp_path / "b.sarif").write_text("{}", encoding="utf-8")
        (tmp_path / "note.txt").write_text("ignored", encoding="utf-8")
        result = _collect([tmp_path])
        assert sorted(result) == sorted([tmp_path / "a.sarif", tmp_path / "b.sarif"])

    def test_directory_recursive(self, tmp_path: Path) -> None:
        """Directory recursive."""
        sub = tmp_path / "sub"
        sub.mkdir()
        (sub / "c.sarif").write_text("{}", encoding="utf-8")
        result = _collect([tmp_path])
        assert tmp_path / "sub" / "c.sarif" in result

    def test_empty_directory_warns(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Empty directory warns."""
        _collect([tmp_path])
        assert "no .sarif files" in capsys.readouterr().err

    def test_mixed_files_and_dirs(self, tmp_path: Path) -> None:
        """Mixed files and dirs."""
        f = tmp_path / "direct.sarif"
        f.write_text("{}", encoding="utf-8")
        sub = tmp_path / "sub"
        sub.mkdir()
        (sub / "nested.sarif").write_text("{}", encoding="utf-8")
        result = _collect([f, sub])
        assert f in result
        assert sub / "nested.sarif" in result

    def test_sorted_order_within_directory(self, tmp_path: Path) -> None:
        """Sorted order within directory."""
        for name in ("c.sarif", "a.sarif", "b.sarif"):
            (tmp_path / name).write_text("{}", encoding="utf-8")
        result = _collect([tmp_path])
        assert result == sorted(result)


# ---------------------------------------------------------------------------
# _process_sarif
# ---------------------------------------------------------------------------


class TestProcessSarif:
    """Tests for TestProcessSarif."""

    def test_basic_result_formatted(self, tmp_path: Path) -> None:
        """Basic result formatted."""
        f = _write_sarif(tmp_path / "r.sarif", [_make_result()])
        lines = list(_process(f))
        assert len(lines) == 1
        assert "py/sql-injection" in lines[0]
        assert "error/new" in lines[0]
        assert "src/app.py:10" in lines[0]
        assert "Possible SQL injection." in lines[0]

    def test_uri_only_when_no_line(self, tmp_path: Path) -> None:
        """Uri only when no line."""
        f = _write_sarif(tmp_path / "r.sarif", [_make_result(start_line=None)])
        lines = list(_process(f))
        assert "src/app.py" in lines[0]
        assert ":None" not in lines[0]

    def test_no_locations_shows_unknown(self, tmp_path: Path) -> None:
        """No locations shows unknown."""
        result = _make_result()
        result["locations"] = []
        f = tmp_path / "r.sarif"
        f.write_text(json.dumps(_make_sarif([result])), encoding="utf-8")
        lines = list(_process(f))
        assert "(unknown location)" in lines[0]

    def test_min_level_filters_below_threshold(self, tmp_path: Path) -> None:
        """Min level filters below threshold."""
        f = _write_sarif(
            tmp_path / "r.sarif",
            [
                _make_result(level="note"),
                _make_result(level="warning"),
                _make_result(level="error"),
            ],
        )
        lines = list(_process(f, min_level="warning"))
        assert len(lines) == 2
        assert all("note" not in ln for ln in lines)

    def test_min_level_none_shows_all(self, tmp_path: Path) -> None:
        """Min level none shows all."""
        f = _write_sarif(
            tmp_path / "r.sarif",
            [_make_result(level="none"), _make_result(level="note")],
        )
        lines = list(_process(f, min_level="none"))
        assert len(lines) == 2

    def test_baseline_filter_exact(self, tmp_path: Path) -> None:
        """Baseline filter exact."""
        f = _write_sarif(
            tmp_path / "r.sarif",
            [
                _make_result(baseline_state="new"),
                _make_result(baseline_state="absent"),
                _make_result(baseline_state="unchanged"),
            ],
        )
        lines = list(_process(f, baseline_filter={"new"}))
        assert len(lines) == 1
        assert "new" in lines[0]

    def test_baseline_filter_multiple(self, tmp_path: Path) -> None:
        """Baseline filter multiple."""
        f = _write_sarif(
            tmp_path / "r.sarif",
            [
                _make_result(baseline_state="new"),
                _make_result(baseline_state="absent"),
                _make_result(baseline_state="unchanged"),
            ],
        )
        lines = list(_process(f, baseline_filter={"new", "absent"}))
        assert len(lines) == 2

    def test_baseline_filter_none_shows_all(self, tmp_path: Path) -> None:
        """Baseline filter none shows all."""
        f = _write_sarif(
            tmp_path / "r.sarif",
            [_make_result(baseline_state="new"), _make_result(baseline_state="unchanged")],
        )
        lines = list(_process(f, baseline_filter=None))
        assert len(lines) == 2

    def test_message_truncated(self, tmp_path: Path) -> None:
        """Message truncated."""
        long_msg = "x" * 300
        f = _write_sarif(tmp_path / "r.sarif", [_make_result(message=long_msg)])
        lines = list(_process(f, max_message=50))
        msg_part = lines[0].split("— ", 1)[1]
        assert len(msg_part) <= 50
        assert msg_part.endswith("…")

    def test_message_whitespace_collapsed(self, tmp_path: Path) -> None:
        """Message whitespace collapsed."""
        f = _write_sarif(tmp_path / "r.sarif", [_make_result(message="a  b\n  c")])
        lines = list(_process(f))
        assert "a b c" in lines[0]

    def test_empty_results_yields_nothing(self, tmp_path: Path) -> None:
        """Empty results yields nothing."""
        f = _write_sarif(tmp_path / "r.sarif", [])
        assert not list(_process(f))

    def test_multiple_runs_merged(self, tmp_path: Path) -> None:
        """Multiple runs merged."""
        sarif = {
            "version": "2.1.0",
            "runs": [
                {
                    "tool": {"driver": {"name": "CodeQL"}},
                    "results": [_make_result(rule_id="py/r1")],
                },
                {
                    "tool": {"driver": {"name": "CodeQL"}},
                    "results": [_make_result(rule_id="cpp/r2")],
                },
            ],
        }
        f = tmp_path / "r.sarif"
        f.write_text(json.dumps(sarif), encoding="utf-8")
        lines = list(_process(f))
        rule_ids = {ln.split()[0] for ln in lines}
        assert rule_ids == {"py/r1", "cpp/r2"}

    def test_invalid_json_raises_value_error(self, tmp_path: Path) -> None:
        """Invalid json raises value error."""
        f = tmp_path / "bad.sarif"
        f.write_text("{not valid json}", encoding="utf-8")
        with pytest.raises(ValueError, match="Invalid JSON"):
            list(_process(f))

    def test_non_object_json_raises_value_error(self, tmp_path: Path) -> None:
        """Non object json raises value error."""
        f = tmp_path / "bad.sarif"
        f.write_text("[1, 2, 3]", encoding="utf-8")
        with pytest.raises(ValueError, match="Not a SARIF document"):
            list(_process(f))

    @pytest.mark.skipif(os.getuid() == 0, reason="root bypasses file permission checks")
    def test_unreadable_file_raises_oserror(self, tmp_path: Path) -> None:
        """Unreadable file raises oserror."""
        f = tmp_path / "locked.sarif"
        f.write_text("{}", encoding="utf-8")
        f.chmod(0o000)
        try:
            with pytest.raises(OSError, match="Cannot read"):
                list(_process(f))
        finally:
            f.chmod(0o644)

    def test_missing_message_field(self, tmp_path: Path) -> None:
        """Missing message field."""
        result = _make_result()
        del result["message"]
        f = tmp_path / "r.sarif"
        f.write_text(json.dumps(_make_sarif([result])), encoding="utf-8")
        lines = list(_process(f))
        # Should still yield a line without crashing; message portion is empty
        assert len(lines) == 1
        assert "— " in lines[0]

    def test_missing_rule_id(self, tmp_path: Path) -> None:
        """Missing rule id."""
        result = _make_result()
        del result["ruleId"]
        f = tmp_path / "r.sarif"
        f.write_text(json.dumps(_make_sarif([result])), encoding="utf-8")
        lines = list(_process(f))
        assert "<no rule>" in lines[0]

    def test_missing_level_defaults_to_none(self, tmp_path: Path) -> None:
        """Missing level defaults to none."""
        result = _make_result()
        del result["level"]
        f = tmp_path / "r.sarif"
        f.write_text(json.dumps(_make_sarif([result])), encoding="utf-8")
        lines = list(_process(f, min_level="none"))
        assert "none/" in lines[0]

    def test_missing_baseline_defaults_to_unchanged(self, tmp_path: Path) -> None:
        """Missing baseline defaults to unchanged."""
        result = _make_result()
        del result["baselineState"]
        f = tmp_path / "r.sarif"
        f.write_text(json.dumps(_make_sarif([result])), encoding="utf-8")
        lines = list(_process(f))
        assert "/unchanged" in lines[0]


# ---------------------------------------------------------------------------
# main()
# ---------------------------------------------------------------------------


class TestMain:
    """Tests for TestMain."""

    def _sarif_file(self, tmp_path: Path, results: list[dict]) -> Path:
        return _write_sarif(tmp_path / "results.sarif", results)

    def test_returns_zero_on_success(self, tmp_path: Path) -> None:
        """Returns zero on success."""
        f = self._sarif_file(tmp_path, [_make_result()])
        rc = _main([str(f)])
        assert rc == 0

    def test_missing_file_returns_one(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Missing file returns one."""
        rc = _main([str(tmp_path / "nonexistent.sarif")])
        assert rc == 1
        assert "Error" in capsys.readouterr().err

    def test_bad_json_returns_one(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Bad json returns one."""
        f = tmp_path / "bad.sarif"
        f.write_text("{bad json}", encoding="utf-8")
        rc = _main([str(f)])
        assert rc == 1
        assert "Error" in capsys.readouterr().err

    def test_output_contains_result_line(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Output contains result line."""
        f = self._sarif_file(tmp_path, [_make_result(rule_id="py/sql-injection")])
        _main([str(f)])
        out = capsys.readouterr().out
        assert "py/sql-injection" in out

    def test_total_line_printed(self, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
        """Total line printed."""
        f = self._sarif_file(tmp_path, [_make_result(), _make_result()])
        _main([str(f)])
        out = capsys.readouterr().out
        assert "Total alerts: 2" in out

    def test_level_filter_applied(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Level filter applied."""
        f = self._sarif_file(
            tmp_path,
            [_make_result(level="note"), _make_result(level="error")],
        )
        _main([str(f), "--level", "warning"])
        out = capsys.readouterr().out
        assert "Total alerts: 1" in out
        assert "note" not in out

    def test_baseline_filter_applied(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Baseline filter applied."""
        f = self._sarif_file(
            tmp_path,
            [_make_result(baseline_state="new"), _make_result(baseline_state="unchanged")],
        )
        _main([str(f), "--baseline", "new"])
        out = capsys.readouterr().out
        assert "Total alerts: 1" in out
        assert "unchanged" not in out

    def test_baseline_filter_repeated(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Baseline filter repeated."""
        f = self._sarif_file(
            tmp_path,
            [
                _make_result(baseline_state="new"),
                _make_result(baseline_state="absent"),
                _make_result(baseline_state="unchanged"),
            ],
        )
        _main([str(f), "--baseline", "new", "--baseline", "absent"])
        out = capsys.readouterr().out
        assert "Total alerts: 2" in out

    def test_directory_input(self, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
        """Directory input."""
        sarif_dir = tmp_path / "sarif"
        _write_sarif(sarif_dir / "a.sarif", [_make_result(rule_id="py/r1")])
        _write_sarif(sarif_dir / "b.sarif", [_make_result(rule_id="py/r2")])
        rc = _main([str(sarif_dir)])
        assert rc == 0
        out = capsys.readouterr().out
        assert "py/r1" in out
        assert "py/r2" in out
        assert "Total alerts: 2" in out

    def test_multiple_files(self, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
        """Multiple files."""
        f1 = _write_sarif(tmp_path / "a.sarif", [_make_result(rule_id="r1")])
        f2 = _write_sarif(tmp_path / "b.sarif", [_make_result(rule_id="r2")])
        _main([str(f1), str(f2)])
        out = capsys.readouterr().out
        assert "Total alerts: 2" in out

    def test_header_line_printed_per_file(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Header line printed per file."""
        f = self._sarif_file(tmp_path, [_make_result()])
        _main([str(f)])
        out = capsys.readouterr().out
        assert f"== {f} ==" in out

    def test_partial_failure_still_processes_good_files(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Partial failure still processes good files."""
        good = self._sarif_file(tmp_path, [_make_result(rule_id="good/rule")])
        bad = tmp_path / "bad.sarif"
        bad.write_text("{broken}", encoding="utf-8")
        rc = _main([str(bad), str(good)])
        assert rc == 1  # error due to bad file
        out = capsys.readouterr().out
        assert "good/rule" in out  # good file was still processed

    def test_empty_sarif_shows_zero_total(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Empty sarif shows zero total."""
        f = self._sarif_file(tmp_path, [])
        _main([str(f)])
        assert "Total alerts: 0" in capsys.readouterr().out

    def test_max_message_arg(self, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
        """Max message arg."""
        long_msg = "y" * 300
        f = self._sarif_file(tmp_path, [_make_result(message=long_msg)])
        _main([str(f), "--max-message", "30"])
        out = capsys.readouterr().out
        # Take only the result line (first line after the "— " separator)
        msg_part = out.split("— ", 1)[1].splitlines()[0]
        assert len(msg_part) <= 30
        assert msg_part.endswith("…")

    def test_sys_not_imported_lazily(self) -> None:
        """`sys` must be imported at module level.

        Missing-file errors must work when main() is called programmatically
        (not via the ``__main__`` guard).
        """
        assert hasattr(sarif_alerts, "__file__")
        src = Path(sarif_alerts.__file__ or "").read_text(encoding="utf-8")
        # 'import sys' must appear before any function definition that uses it
        import_pos = src.index("import sys")
        main_pos = src.index("def main(")
        assert import_pos < main_pos, "'import sys' must appear at module level before main()"
