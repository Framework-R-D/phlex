"""Tests for coverage-related scripts.

Covers:
- normalize_coverage_lcov.py: _relative_subpath, _is_repo_content, normalize, main
- normalize_coverage_xml.py:  _relative_subpath, normalize, main
- export_llvm_lcov.py:        build_parser, main
- create_coverage_symlinks.py: should_link, iter_source_files, create_symlinks,
                               parse_args, main
"""

from __future__ import annotations

import subprocess
import textwrap
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

# sys.path is set up by scripts/test/conftest.py.
import create_coverage_symlinks as ccs  # noqa: E402
import export_llvm_lcov as ell  # noqa: E402
import normalize_coverage_lcov as ncl  # noqa: E402
import normalize_coverage_xml as ncx  # noqa: E402

# ===========================================================================
# normalize_coverage_lcov
# ===========================================================================


class TestNclRelativeSubpath:
    """_relative_subpath from normalize_coverage_lcov."""

    def test_direct_subpath(self, tmp_path: Path) -> None:
        """Direct subpath."""
        base = tmp_path / "repo"
        path = base / "src" / "foo.cpp"
        base.mkdir()
        result = ncl._relative_subpath(path, base)
        assert result == Path("src/foo.cpp")

    def test_path_outside_base_returns_none(self, tmp_path: Path) -> None:
        """Path outside base returns none."""
        base = tmp_path / "repo"
        path = tmp_path / "other" / "foo.cpp"
        result = ncl._relative_subpath(path, base)
        assert result is None

    def test_none_base_returns_none(self, tmp_path: Path) -> None:
        """None base returns none."""
        assert ncl._relative_subpath(tmp_path / "foo", None) is None

    def test_resolved_symlink_path(self, tmp_path: Path) -> None:
        """Resolved symlink path."""
        real = tmp_path / "real"
        real.mkdir()
        link = tmp_path / "link"
        link.symlink_to(real)
        child = real / "bar.cpp"
        child.write_text("", encoding="utf-8")
        # path is under link (symlink), base is real — should still resolve
        result = ncl._relative_subpath(link / "bar.cpp", real)
        assert result is not None


class TestNclIsRepoContent:
    """_is_repo_content from normalize_coverage_lcov."""

    def test_phlex_prefix_accepted(self) -> None:
        """Phlex prefix accepted."""
        assert ncl._is_repo_content(Path("phlex/src/foo.cpp"))

    def test_plugins_prefix_accepted(self) -> None:
        """Plugins prefix accepted."""
        assert ncl._is_repo_content(Path("plugins/py/bar.py"))

    def test_form_prefix_accepted(self) -> None:
        """Form prefix accepted."""
        assert ncl._is_repo_content(Path("form/src/baz.c"))

    def test_build_clang_prefix_accepted(self) -> None:
        """Build clang prefix accepted."""
        assert ncl._is_repo_content(Path("build-clang/CMakeFiles/foo.cpp"))

    def test_coverage_generated_in_parts_accepted(self) -> None:
        """Coverage generated in parts accepted."""
        assert ncl._is_repo_content(Path("some/.coverage-generated/foo.cpp"))

    def test_unknown_prefix_rejected(self) -> None:
        """Unknown prefix rejected."""
        assert not ncl._is_repo_content(Path("external/lib/foo.h"))

    def test_empty_path_rejected(self) -> None:
        """Empty path rejected."""
        assert not ncl._is_repo_content(Path(""))


class TestNclNormalize:
    """normalize() from normalize_coverage_lcov."""

    def _make_lcov(self, sf_path: str, extra_lines: str = "") -> str:
        return textwrap.dedent(
            f"""\
            TN:
            SF:{sf_path}
            FN:1,main
            FNDA:1,main
            FNF:1
            FNH:1
            DA:1,1
            LF:1
            LH:1
            {extra_lines}end_of_record
            """
        )

    def test_absolute_path_rewritten_relative(self, tmp_path: Path) -> None:
        """Absolute path rewritten relative."""
        repo = tmp_path / "repo"
        repo.mkdir()
        src = repo / "phlex" / "src" / "foo.cpp"
        src.parent.mkdir(parents=True)
        src.write_text("int main(){}", encoding="utf-8")

        report = tmp_path / "coverage.info"
        report.write_text(self._make_lcov(str(src)), encoding="utf-8")

        missing, external = ncl.normalize(report, repo)
        assert missing == []
        assert external == []
        content = report.read_text()
        assert "SF:phlex/src/foo.cpp" in content

    def test_external_file_excluded_from_output(self, tmp_path: Path) -> None:
        """External file excluded from output."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = tmp_path / "coverage.info"
        report.write_text(self._make_lcov("/usr/include/stdio.h"), encoding="utf-8")

        missing, external = ncl.normalize(report, repo)
        assert len(external) == 1
        # Record must be dropped (no SF: for the external file)
        content = report.read_text()
        assert "SF:" not in content

    def test_missing_file_in_repo_reported(self, tmp_path: Path) -> None:
        """Missing file in repo reported."""
        repo = tmp_path / "repo"
        repo.mkdir()
        (repo / "phlex").mkdir()
        report = tmp_path / "coverage.info"
        # file is within the repo prefix but does not exist on disk
        absent = str(repo / "phlex" / "absent.cpp")
        report.write_text(self._make_lcov(absent), encoding="utf-8")

        missing, external = ncl.normalize(report, repo)
        assert len(missing) == 1
        assert "phlex/absent.cpp" in missing[0]

    def test_relative_sf_resolved_against_coverage_root(self, tmp_path: Path) -> None:
        """Relative sf resolved against coverage root."""
        repo = tmp_path / "repo"
        repo.mkdir()
        coverage_root = repo / "phlex"
        coverage_root.mkdir()
        src = coverage_root / "foo.cpp"
        src.write_text("", encoding="utf-8")

        report = tmp_path / "coverage.info"
        # SF path is relative to coverage_root
        report.write_text(self._make_lcov("foo.cpp"), encoding="utf-8")

        missing, external = ncl.normalize(report, repo, coverage_root=coverage_root)
        assert missing == []
        content = report.read_text()
        assert "SF:phlex/foo.cpp" in content

    def test_absolute_paths_flag(self, tmp_path: Path) -> None:
        """Absolute paths flag."""
        repo = tmp_path / "repo"
        repo.mkdir()
        src = repo / "phlex" / "x.cpp"
        src.parent.mkdir()
        src.write_text("", encoding="utf-8")

        report = tmp_path / "coverage.info"
        report.write_text(self._make_lcov(str(src)), encoding="utf-8")

        ncl.normalize(report, repo, absolute_paths=True)
        content = report.read_text()
        # path must be absolute
        assert content.startswith("TN:") or "SF:/" in content
        assert "SF:/" in content

    def test_record_without_sf_preserved(self, tmp_path: Path) -> None:
        """Record without sf preserved."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = tmp_path / "coverage.info"
        # A record with no SF line (unusual but defensively handled)
        report.write_text("TN:\nend_of_record\n", encoding="utf-8")

        missing, external = ncl.normalize(report, repo)
        assert missing == []
        assert external == []

    def test_empty_sf_value_preserved(self, tmp_path: Path) -> None:
        """Empty sf value preserved."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = tmp_path / "coverage.info"
        report.write_text("TN:\nSF:\nend_of_record\n", encoding="utf-8")

        missing, external = ncl.normalize(report, repo)
        assert missing == []
        assert external == []

    def test_trailing_lines_without_end_of_record_handled(self, tmp_path: Path) -> None:
        """Trailing lines without end of record handled."""
        repo = tmp_path / "repo"
        repo.mkdir()
        # No end_of_record terminator — should not crash
        report = tmp_path / "coverage.info"
        report.write_text("TN:\nSF:\nDA:1,1\n", encoding="utf-8")
        ncl.normalize(report, repo)  # must not raise


class TestNclMain:
    """main() from normalize_coverage_lcov."""

    def _make_report(self, tmp_path: Path, sf_path: str) -> Path:
        report = tmp_path / "coverage.info"
        report.write_text(f"TN:\nSF:{sf_path}\nend_of_record\n", encoding="utf-8")
        return report

    def test_success_returns_zero(self, tmp_path: Path) -> None:
        """Success returns zero."""
        repo = tmp_path / "repo"
        repo.mkdir()
        src = repo / "phlex" / "ok.cpp"
        src.parent.mkdir()
        src.write_text("", encoding="utf-8")
        report = self._make_report(tmp_path, str(src))
        rc = ncl.main([str(report), "--repo-root", str(repo)])
        assert rc == 0

    def test_missing_file_returns_nonzero(self, tmp_path: Path) -> None:
        """Missing file returns nonzero."""
        repo = tmp_path / "repo"
        repo.mkdir()
        (repo / "phlex").mkdir()
        report = self._make_report(tmp_path, str(repo / "phlex" / "missing.cpp"))
        rc = ncl.main([str(report), "--repo-root", str(repo)])
        assert rc != 0

    def test_external_file_warns_to_stderr(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """External file warns to stderr."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = self._make_report(tmp_path, "/usr/include/stdio.h")
        ncl.main([str(report), "--repo-root", str(repo)])
        assert "outside the repository" in capsys.readouterr().err


# ===========================================================================
# normalize_coverage_xml
# ===========================================================================


def _make_xml(
    filenames: list[str],
    source: str | None = None,
) -> str:
    sources = f"<sources><source>{source or '.'}</source></sources>" if source else ""
    classes = "".join(
        f'<class filename="{fn}" line-rate="1.0" branch-rate="1.0" complexity="0"></class>'
        for fn in filenames
    )
    return (
        '<?xml version="1.0" ?>\n'
        '<coverage version="7.0" timestamp="0" lines-valid="1" '
        'lines-covered="1" line-rate="1.0" branches-valid="0" '
        'branches-covered="0" branch-rate="0" complexity="0">\n'
        f"{sources}\n"
        f"<packages><package><classes>{classes}</classes></package></packages>\n"
        "</coverage>\n"
    )


class TestNcxRelativeSubpath:
    """_relative_subpath from normalize_coverage_xml."""

    def test_subpath_within_base(self, tmp_path: Path) -> None:
        """Subpath within base."""
        base = tmp_path / "repo"
        path = base / "src" / "foo.cpp"
        result = ncx._relative_subpath(path, base)
        assert result == Path("src/foo.cpp")

    def test_outside_base_returns_none(self, tmp_path: Path) -> None:
        """Outside base returns none."""
        base = tmp_path / "repo"
        path = tmp_path / "other" / "foo.cpp"
        assert ncx._relative_subpath(path, base) is None

    def test_none_base_returns_none(self, tmp_path: Path) -> None:
        """None base returns none."""
        assert ncx._relative_subpath(tmp_path / "foo", None) is None


class TestNcxNormalize:
    """normalize() from normalize_coverage_xml."""

    def test_absolute_path_rewritten_relative(self, tmp_path: Path) -> None:
        """Absolute path rewritten relative."""
        repo = tmp_path / "repo"
        repo.mkdir()
        src = repo / "phlex" / "foo.cpp"
        src.parent.mkdir()
        src.write_text("", encoding="utf-8")

        report = tmp_path / "coverage.xml"
        report.write_text(_make_xml([str(src)]), encoding="utf-8")

        missing, external = ncx.normalize(report, repo)
        assert missing == []
        assert external == []
        tree = ET.parse(report)
        filenames = [cls.get("filename") for cls in tree.findall(".//class")]
        assert filenames == ["phlex/foo.cpp"]

    def test_external_file_reported(self, tmp_path: Path) -> None:
        """External file reported."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = tmp_path / "coverage.xml"
        report.write_text(_make_xml(["/usr/include/stdio.h"]), encoding="utf-8")

        missing, external = ncx.normalize(report, repo)
        assert len(external) == 1
        assert "stdio.h" in external[0]

    def test_missing_file_reported(self, tmp_path: Path) -> None:
        """Missing file reported."""
        repo = tmp_path / "repo"
        repo.mkdir()
        (repo / "phlex").mkdir()
        report = tmp_path / "coverage.xml"
        # relative filename that does not exist on disk
        report.write_text(_make_xml(["phlex/absent.cpp"]), encoding="utf-8")

        missing, external = ncx.normalize(report, repo)
        assert len(missing) == 1

    def test_source_element_normalized(self, tmp_path: Path) -> None:
        """Source element normalized."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = tmp_path / "coverage.xml"
        report.write_text(_make_xml([], source="/old/path"), encoding="utf-8")

        ncx.normalize(report, repo)
        tree = ET.parse(report)
        sources = tree.findall("sources/source")
        assert len(sources) == 1
        assert sources[0].text == str(repo)

    def test_custom_source_dir(self, tmp_path: Path) -> None:
        """Custom source dir."""
        repo = tmp_path / "repo"
        repo.mkdir()
        custom_src = tmp_path / "custom"
        report = tmp_path / "coverage.xml"
        report.write_text(_make_xml([]), encoding="utf-8")

        ncx.normalize(report, repo, source_dir=custom_src)
        tree = ET.parse(report)
        sources = tree.findall("sources/source")
        assert sources[0].text == str(custom_src)

    def test_relative_filename_within_coverage_root(self, tmp_path: Path) -> None:
        """Relative filename within coverage root."""
        repo = tmp_path / "repo"
        repo.mkdir()
        coverage_root = repo / "phlex"
        coverage_root.mkdir()
        src = coverage_root / "foo.cpp"
        src.write_text("", encoding="utf-8")

        report = tmp_path / "coverage.xml"
        # filename is relative to coverage_root
        report.write_text(_make_xml(["foo.cpp"]), encoding="utf-8")

        missing, external = ncx.normalize(report, repo, coverage_root=coverage_root)
        assert missing == []

    def test_path_map_applied(self, tmp_path: Path) -> None:
        """Path map applied."""
        repo = tmp_path / "repo"
        repo.mkdir()
        from_dir = tmp_path / "generated"
        from_dir.mkdir()
        to_dir = repo / "build-clang"
        to_dir.mkdir()
        src = from_dir / "auto.cpp"
        src.write_text("", encoding="utf-8")

        report = tmp_path / "coverage.xml"
        report.write_text(_make_xml([str(src)]), encoding="utf-8")

        missing, external = ncx.normalize(report, repo, path_maps=[(from_dir, to_dir)])
        assert external == []

    def test_no_classes_produces_no_errors(self, tmp_path: Path) -> None:
        """No classes produces no errors."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = tmp_path / "coverage.xml"
        report.write_text(_make_xml([]), encoding="utf-8")
        missing, external = ncx.normalize(report, repo)
        assert missing == []
        assert external == []


class TestNcxMain:
    """main() from normalize_coverage_xml."""

    def _report(self, tmp_path: Path, filenames: list[str]) -> Path:
        report = tmp_path / "coverage.xml"
        report.write_text(_make_xml(filenames), encoding="utf-8")
        return report

    def test_success_returns_zero(self, tmp_path: Path) -> None:
        """Success returns zero."""
        repo = tmp_path / "repo"
        repo.mkdir()
        src = repo / "phlex" / "ok.cpp"
        src.parent.mkdir()
        src.write_text("", encoding="utf-8")
        report = self._report(tmp_path, [str(src)])
        rc = ncx.main([str(report), "--repo-root", str(repo)])
        assert rc == 0

    def test_external_returns_nonzero(self, tmp_path: Path) -> None:
        """External returns nonzero."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = self._report(tmp_path, ["/usr/include/stdio.h"])
        rc = ncx.main([str(report), "--repo-root", str(repo)])
        assert rc != 0

    def test_missing_file_returns_nonzero(self, tmp_path: Path) -> None:
        """Missing file returns nonzero."""
        repo = tmp_path / "repo"
        repo.mkdir()
        (repo / "phlex").mkdir()
        report = self._report(tmp_path, [str(repo / "phlex" / "missing.cpp")])
        rc = ncx.main([str(report), "--repo-root", str(repo)])
        assert rc != 0

    def test_invalid_path_map_raises(self, tmp_path: Path) -> None:
        """Invalid path map raises."""
        repo = tmp_path / "repo"
        repo.mkdir()
        report = self._report(tmp_path, [])
        with pytest.raises(SystemExit):
            ncx.main([str(report), "--repo-root", str(repo), "--path-map", "noequalssign"])

    def test_path_map_parsed(self, tmp_path: Path) -> None:
        """Path map parsed."""
        repo = tmp_path / "repo"
        repo.mkdir()
        src = repo / "phlex" / "ok.cpp"
        src.parent.mkdir()
        src.write_text("", encoding="utf-8")
        report = self._report(tmp_path, [str(src)])
        # valid path map: should not raise
        rc = ncx.main(
            [
                str(report),
                "--repo-root",
                str(repo),
                "--path-map",
                f"{tmp_path}={repo}",
            ]
        )
        assert rc == 0


# ===========================================================================
# export_llvm_lcov
# ===========================================================================


class TestEllBuildParser:
    """build_parser() from export_llvm_lcov."""

    def test_parser_returns_parser(self) -> None:
        """Parser returns parser."""
        import argparse

        p = ell.build_parser()
        assert isinstance(p, argparse.ArgumentParser)

    def test_required_args_parsed(self) -> None:
        """Required args parsed."""
        p = ell.build_parser()
        args = p.parse_args(["out.info", "/usr/bin/llvm-cov", "export", "-instr-profile=foo"])
        assert args.output == "out.info"
        assert args.llvm_cov == "/usr/bin/llvm-cov"
        assert "export" in args.llvm_cov_args

    def test_no_llvm_cov_args_produces_empty_list(self) -> None:
        """No llvm cov args produces empty list."""
        p = ell.build_parser()
        args = p.parse_args(["out.info", "/usr/bin/llvm-cov"])
        assert args.llvm_cov_args == []


class TestEllMain:
    """main() from export_llvm_lcov."""

    def test_success_writes_output_file(self, tmp_path: Path) -> None:
        """Success writes output file."""
        out = tmp_path / "out.info"
        with patch("subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            rc = ell.main([str(out), "llvm-cov", "export", "-foo"])
        assert rc == 0
        assert mock_run.called
        # The output file must be created (even if empty when subprocess is mocked).
        assert out.exists()

    def test_creates_parent_directories(self, tmp_path: Path) -> None:
        """Creates parent directories."""
        out = tmp_path / "nested" / "deep" / "out.info"
        with patch("subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            ell.main([str(out), "llvm-cov", "export", "-foo"])
        assert out.parent.exists()

    def test_subprocess_error_propagates_returncode(self, tmp_path: Path) -> None:
        """Subprocess error propagates returncode."""
        out = tmp_path / "out.info"
        error = subprocess.CalledProcessError(returncode=42, cmd=["llvm-cov"])
        with patch("subprocess.run", side_effect=error):
            rc = ell.main([str(out), "llvm-cov", "export", "-foo"])
        assert rc == 42

    def test_missing_llvm_cov_args_exits_with_error(self, tmp_path: Path) -> None:
        """Missing llvm cov args exits with error."""
        out = tmp_path / "out.info"
        with pytest.raises(SystemExit):
            ell.main([str(out), "llvm-cov"])

    def test_command_includes_llvm_cov_args(self, tmp_path: Path) -> None:
        """Command includes llvm cov args."""
        out = tmp_path / "out.info"
        captured: list[Any] = []

        def _run(cmd: list[str], **kwargs: Any) -> MagicMock:
            captured.append(cmd)
            return MagicMock(returncode=0)

        with patch("subprocess.run", side_effect=_run):
            ell.main([str(out), "llvm-cov", "export", "-instr-profile=foo", "binary"])

        assert captured[0] == ["llvm-cov", "export", "-instr-profile=foo", "binary"]


# ===========================================================================
# create_coverage_symlinks
# ===========================================================================


class TestCcsShouldLink:
    """should_link() from create_coverage_symlinks."""

    def test_cpp_file_accepted(self, tmp_path: Path) -> None:
        """Cpp file accepted."""
        f = tmp_path / "foo.cpp"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_h_file_accepted(self, tmp_path: Path) -> None:
        """H file accepted."""
        f = tmp_path / "foo.h"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_c_file_accepted(self, tmp_path: Path) -> None:
        """C file accepted."""
        f = tmp_path / "foo.c"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_icc_file_accepted(self, tmp_path: Path) -> None:
        """Icc file accepted."""
        f = tmp_path / "foo.icc"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_tcc_file_accepted(self, tmp_path: Path) -> None:
        """Tcc file accepted."""
        f = tmp_path / "foo.tcc"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_i_file_accepted(self, tmp_path: Path) -> None:
        """I file accepted."""
        f = tmp_path / "foo.i"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_ii_file_accepted(self, tmp_path: Path) -> None:
        """Ii file accepted."""
        f = tmp_path / "foo.ii"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_txt_file_rejected(self, tmp_path: Path) -> None:
        """Txt file rejected."""
        f = tmp_path / "readme.txt"
        f.write_text("", encoding="utf-8")
        assert not ccs.should_link(f)

    def test_py_file_rejected(self, tmp_path: Path) -> None:
        """Py file rejected."""
        f = tmp_path / "script.py"
        f.write_text("", encoding="utf-8")
        assert not ccs.should_link(f)

    def test_directory_rejected(self, tmp_path: Path) -> None:
        """Directory rejected."""
        d = tmp_path / "dir"
        d.mkdir()
        assert not ccs.should_link(d)

    def test_nonexistent_file_rejected(self, tmp_path: Path) -> None:
        """Nonexistent file rejected."""
        assert not ccs.should_link(tmp_path / "ghost.cpp")

    def test_cxx_extension_accepted(self, tmp_path: Path) -> None:
        """Cxx extension accepted."""
        f = tmp_path / "bar.cxx"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)

    def test_hpp_extension_accepted(self, tmp_path: Path) -> None:
        """Hpp extension accepted."""
        f = tmp_path / "bar.hpp"
        f.write_text("", encoding="utf-8")
        assert ccs.should_link(f)


class TestCcsIterSourceFiles:
    """iter_source_files() from create_coverage_symlinks."""

    def test_yields_cpp_files(self, tmp_path: Path) -> None:
        """Yields cpp files."""
        (tmp_path / "a.cpp").write_text("", encoding="utf-8")
        (tmp_path / "b.h").write_text("", encoding="utf-8")
        (tmp_path / "c.txt").write_text("", encoding="utf-8")
        result = list(ccs.iter_source_files(tmp_path))
        names = {p.name for p in result}
        assert "a.cpp" in names
        assert "b.h" in names
        assert "c.txt" not in names

    def test_recurses_into_subdirectories(self, tmp_path: Path) -> None:
        """Recurses into subdirectories."""
        sub = tmp_path / "sub"
        sub.mkdir()
        (sub / "nested.cpp").write_text("", encoding="utf-8")
        result = list(ccs.iter_source_files(tmp_path))
        assert any(p.name == "nested.cpp" for p in result)

    def test_empty_directory_yields_nothing(self, tmp_path: Path) -> None:
        """Empty directory yields nothing."""
        assert list(ccs.iter_source_files(tmp_path)) == []


class TestCcsCreateSymlinks:
    """create_symlinks() from create_coverage_symlinks."""

    def test_symlinks_created_for_cpp_files(self, tmp_path: Path) -> None:
        """Symlinks created for cpp files."""
        build = tmp_path / "build"
        build.mkdir()
        (build / "foo.cpp").write_text("int main(){}", encoding="utf-8")
        output = tmp_path / "links"

        ccs.create_symlinks(build, output)

        link = output / "foo.cpp"
        assert link.is_symlink()
        assert link.resolve() == (build / "foo.cpp").resolve()

    def test_non_source_files_not_linked(self, tmp_path: Path) -> None:
        """Non source files not linked."""
        build = tmp_path / "build"
        build.mkdir()
        (build / "readme.txt").write_text("hello", encoding="utf-8")
        output = tmp_path / "links"

        ccs.create_symlinks(build, output)
        assert not (output / "readme.txt").exists()

    def test_output_dir_recreated_if_exists(self, tmp_path: Path) -> None:
        """Output dir recreated if exists."""
        build = tmp_path / "build"
        build.mkdir()
        (build / "a.cpp").write_text("", encoding="utf-8")
        output = tmp_path / "links"
        output.mkdir()
        # Pre-existing stale file in output
        (output / "stale.cpp").write_text("", encoding="utf-8")

        ccs.create_symlinks(build, output)
        assert not (output / "stale.cpp").exists()
        assert (output / "a.cpp").is_symlink()

    def test_nested_paths_preserved(self, tmp_path: Path) -> None:
        """Nested paths preserved."""
        build = tmp_path / "build"
        sub = build / "sub"
        sub.mkdir(parents=True)
        (sub / "nested.cpp").write_text("", encoding="utf-8")
        output = tmp_path / "links"

        ccs.create_symlinks(build, output)
        assert (output / "sub" / "nested.cpp").is_symlink()

    def test_existing_symlink_replaced(self, tmp_path: Path) -> None:
        """Existing symlink replaced."""
        build = tmp_path / "build"
        build.mkdir()
        src = build / "foo.cpp"
        src.write_text("v2", encoding="utf-8")
        output = tmp_path / "links"
        output.mkdir()
        # Create an old symlink pointing somewhere else
        old_target = tmp_path / "old.cpp"
        old_target.write_text("v1", encoding="utf-8")
        (output / "foo.cpp").symlink_to(old_target)

        ccs.create_symlinks(build, output)
        # symlink must now point to the build source, not old_target
        assert (output / "foo.cpp").resolve() == src.resolve()


class TestCcsParseArgs:
    """parse_args() from create_coverage_symlinks."""

    def test_required_args_parsed(self, tmp_path: Path) -> None:
        """Required args parsed."""
        argv = ["prog", "--build-root", str(tmp_path), "--output-root", str(tmp_path)]
        args = ccs.parse_args(argv)
        assert args.build_root == str(tmp_path)
        assert args.output_root == str(tmp_path)

    def test_missing_required_arg_raises(self) -> None:
        """Missing required arg raises."""
        with pytest.raises(SystemExit):
            ccs.parse_args(["prog", "--build-root", "/foo"])


class TestCcsMain:
    """main() from create_coverage_symlinks."""

    def test_success_returns_zero(self, tmp_path: Path) -> None:
        """Success returns zero."""
        build = tmp_path / "build"
        build.mkdir()
        (build / "foo.cpp").write_text("", encoding="utf-8")
        output = tmp_path / "out"
        rc = ccs.main(["prog", "--build-root", str(build), "--output-root", str(output)])
        assert rc == 0
        assert (output / "foo.cpp").is_symlink()

    def test_nonexistent_build_root_returns_one(self, tmp_path: Path) -> None:
        """Nonexistent build root returns one."""
        rc = ccs.main(
            [
                "prog",
                "--build-root",
                str(tmp_path / "no_such"),
                "--output-root",
                str(tmp_path / "out"),
            ]
        )
        assert rc == 1
