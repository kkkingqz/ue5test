#!/usr/bin/env python3
"""Validate the canonical Markdown documentation without third-party packages."""

from __future__ import annotations

import argparse
import datetime as dt
import re
import sys
from pathlib import Path
from urllib.parse import unquote


FRONT_MATTER_BOUNDARY = "---"
ARCHIVE_DIRECTORY = "Archive"
# A proposal leaves the active set in one of two directions, and the direction
# is the whole point of keeping it: Archive/ records what was built, Rejected/
# records what was considered and turned down. Both are historical, so both
# carry status 'archived'; separating them keeps "we already decided against
# this" findable instead of lost among the delivered work.
REJECTED_DIRECTORY = "Rejected"
HISTORICAL_DIRECTORIES = (ARCHIVE_DIRECTORY, REJECTED_DIRECTORY)
# Explanatory tiers. They never define architectural rules: at a conflict the
# normative contract wins. Keeping the status tied to the location makes the
# distinction impossible to lose in a move or a copy-paste.
INFORMATIVE_DIRECTORIES = ("Concepts", "Guides", "Status")
INFORMATIVE_FILES = {
    "README.md",
    "ProjectBrief.md",
    "ADR/README.md",
    "Plans/README.md",
    "Proposals/README.md",
}
NORMATIVE_DIRECTORIES = ("Architecture", "UI")
# Header opens every document with the one thing its front matter cannot carry.
# The field differs by document type because the useful question differs: a
# contract is defined by what it owns, a guide by the task it solves.
# Indexes are the navigation themselves and carry no header. Archived plans are
# historical records and are not rewritten.
HEADER_FIELD_BY_DIRECTORY = {
    "Architecture": "Владеет",
    "UI": "Владеет",
    "Concepts": "Объясняет",
    "Guides": "Задача",
    "ADR": "Решение",
    "Proposals": "Предлагает",
    "Plans": "Материализует",
    "Status": "Показывает",
}
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HEADING = re.compile(r"^#{1,6}\s+(.+?)\s*$")
VERSION = re.compile(r"^\d+\.\d+$")
EXTERNAL_SCHEMES = ("http://", "https://", "mailto:")


class Validation:
    def __init__(self, docs_root: Path) -> None:
        self.docs_root = docs_root.resolve()
        self.errors: list[str] = []
        self.metadata: dict[Path, dict[str, object]] = {}

    def fail(self, path: Path, message: str) -> None:
        self.errors.append(f"{path.relative_to(self.docs_root)}: {message}")

    def parse_front_matter(self, path: Path, text: str) -> dict[str, object] | None:
        lines = text.splitlines()
        if not lines or lines[0] != FRONT_MATTER_BOUNDARY:
            self.fail(path, "missing opening front matter boundary")
            return None
        try:
            end = lines.index(FRONT_MATTER_BOUNDARY, 1)
        except ValueError:
            self.fail(path, "missing closing front matter boundary")
            return None

        result: dict[str, object] = {}
        current_list: str | None = None
        for line_number, line in enumerate(lines[1:end], start=2):
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if line.startswith("  - "):
                if current_list is None:
                    self.fail(path, f"line {line_number}: list item has no owning key")
                    continue
                value = line[4:].strip()
                assert isinstance(result[current_list], list)
                result[current_list].append(value)
                continue
            match = re.fullmatch(r"([a-z_]+):(?:\s*(.*))?", line)
            if match is None:
                self.fail(path, f"line {line_number}: unsupported front matter syntax")
                current_list = None
                continue
            key, value = match.groups()
            if key in result:
                self.fail(path, f"line {line_number}: duplicate front matter key '{key}'")
            if value:
                result[key] = value.strip().strip('"')
                current_list = None
            else:
                result[key] = []
                current_list = key
        return result

    def validate_front_matter(self, path: Path, metadata: dict[str, object]) -> None:
        relative = path.relative_to(self.docs_root)
        is_adr = relative.parts[0] == "ADR" and path.name != "README.md"
        is_proposal = relative.parts[0] == "Proposals" and path.name != "README.md"
        is_archived_location = ARCHIVE_DIRECTORY in relative.parts
        is_rejected_location = REJECTED_DIRECTORY in relative.parts
        is_historical_location = is_archived_location or is_rejected_location
        relative_name = relative.as_posix()
        is_informative_location = (
            relative.parts[0] in INFORMATIVE_DIRECTORIES
            or relative_name in INFORMATIVE_FILES
        ) and not is_historical_location
        is_active_plan = (
            relative.parts[0] == "Plans"
            and relative_name != "Plans/README.md"
            and not is_historical_location
        )
        is_normative_location = relative.parts[0] in NORMATIVE_DIRECTORIES

        required = {"title", "status"}
        required |= {"date"} if is_adr else {"version", "updated"}
        if is_proposal:
            required.add("proposal_state")
        for key in sorted(required):
            if not metadata.get(key):
                self.fail(path, f"required front matter key '{key}' is missing or empty")

        status = metadata.get("status")
        allowed_status = (
            {"proposed", "accepted", "superseded", "rejected"}
            if is_adr
            else {"active", "draft", "normative", "deprecated", "archived", "informative"}
        )
        if status and status not in allowed_status:
            self.fail(path, f"invalid status '{status}'")

        # Informative tiers and routers explain, instruct or navigate; active
        # plans describe executable work. Neither is an architecture authority.
        if is_informative_location and status != "informative":
            self.fail(
                path,
                f"informative document or router must use status 'informative', found '{status}'",
            )
        if status == "informative" and not is_informative_location:
            self.fail(
                path,
                "status 'informative' is not allowed at this location",
            )
        if is_active_plan and status != "active":
            self.fail(path, f"active plan must use status 'active', found '{status}'")
        if status == "active" and not is_active_plan:
            self.fail(path, "status 'active' is allowed only for active plan documents")
        if status == "normative" and not is_normative_location:
            self.fail(
                path,
                "status 'normative' is allowed only under Architecture/ or UI/",
            )

        # An archived document is a historical record: it is never normative and
        # never a source of tasks. Keep location and status in sync in both
        # directions so an archived plan cannot silently keep an active status.
        if is_historical_location and status != "archived":
            self.fail(
                path,
                f"document under {' or '.join(d + '/' for d in HISTORICAL_DIRECTORIES)} must use status 'archived', found '{status}'",
            )
        if status == "archived" and not is_historical_location:
            self.fail(
                path,
                f"status 'archived' requires the document to live under {' or '.join(d + '/' for d in HISTORICAL_DIRECTORIES)}",
            )

        version = metadata.get("version")
        if version and (not isinstance(version, str) or VERSION.fullmatch(version) is None):
            self.fail(path, f"version '{version}' must have major.minor form")

        date_value = metadata.get("date" if is_adr else "updated")
        if isinstance(date_value, str):
            try:
                dt.date.fromisoformat(date_value)
            except ValueError:
                self.fail(path, f"invalid ISO date '{date_value}'")

        allowed_proposal_states = {
            "accepted_for_planning", "measurement_required", "implemented", "rejected",
        }
        proposal_state = metadata.get("proposal_state")
        if is_proposal and proposal_state not in allowed_proposal_states:
            self.fail(path, f"invalid proposal_state '{proposal_state}'")

        # A proposal's location is its state. An implemented or rejected
        # proposal left in the active directory keeps showing up as work to do,
        # and the index drifts from the directory within a release. Binding the
        # two in both directions makes the move part of finishing the work.
        if is_proposal:
            expected_directory = {
                "implemented": ARCHIVE_DIRECTORY,
                "rejected": REJECTED_DIRECTORY,
            }.get(str(proposal_state))
            if expected_directory is not None and expected_directory not in relative.parts:
                self.fail(
                    path,
                    f"proposal_state '{proposal_state}' requires the document to live under Proposals/{expected_directory}/",
                )
            if expected_directory is None and is_historical_location:
                self.fail(
                    path,
                    f"proposal under {' or '.join(d + '/' for d in HISTORICAL_DIRECTORIES)} must use "
                    f"proposal_state 'implemented' or 'rejected', found '{proposal_state}'",
                )

        allowed_keys = {
            "title", "status", "version", "updated", "date", "language",
            "depends_on", "decisions", "proposal_state", "superseded_by",
        }
        for key in metadata:
            if key not in allowed_keys:
                self.fail(path, f"unknown front matter key '{key}'")

    @staticmethod
    def anchors(text: str) -> set[str]:
        anchors: set[str] = set()
        counts: dict[str, int] = {}
        in_fence = False
        for line in text.splitlines():
            if line.lstrip().startswith("```"):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            match = HEADING.match(line)
            if match is None:
                continue
            heading = re.sub(r"[`*_~]", "", match.group(1)).strip().lower()
            slug = re.sub(r"[^\w\- ]", "", heading, flags=re.UNICODE)
            slug = re.sub(r"\s+", "-", slug)
            count = counts.get(slug, 0)
            counts[slug] = count + 1
            anchors.add(slug if count == 0 else f"{slug}-{count}")
        return anchors

    def resolve_local_target(self, source: Path, raw_target: str) -> tuple[Path, str] | None:
        target = raw_target.strip()
        if target.startswith("<") and ">" in target:
            target = target[1:target.index(">")]
        else:
            target = target.split(maxsplit=1)[0]
        target = unquote(target)
        if not target or target.startswith("#"):
            return source, target[1:]
        if target.startswith(EXTERNAL_SCHEMES):
            return None
        file_part, separator, anchor = target.partition("#")
        resolved = (source.parent / file_part).resolve()
        return resolved, anchor if separator else ""

    def validate_header(self, path: Path, text: str) -> None:
        relative = path.relative_to(self.docs_root)
        if path.name == "README.md":
            return
        if any(directory in relative.parts for directory in HISTORICAL_DIRECTORIES):
            return
        field = HEADER_FIELD_BY_DIRECTORY.get(relative.parts[0])
        if field is None:
            return
        if f"> **{field}:**" not in text:
            self.fail(
                path,
                f"missing header: first block after the title must state '> **{field}:** …' "
                f"(see Docs/Architecture/README.md)",
            )

    def validate_links(self, path: Path, text: str, texts: dict[Path, str]) -> None:
        in_fence = False
        for line_number, line in enumerate(text.splitlines(), start=1):
            if line.lstrip().startswith("```"):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            for match in MARKDOWN_LINK.finditer(line):
                resolved = self.resolve_local_target(path, match.group(1))
                if resolved is None:
                    continue
                target, anchor = resolved
                try:
                    target.relative_to(self.docs_root)
                except ValueError:
                    self.fail(path, f"line {line_number}: local link escapes Docs: {match.group(1)}")
                    continue
                if not target.is_file():
                    self.fail(path, f"line {line_number}: missing link target: {match.group(1)}")
                    continue
                if anchor and anchor not in self.anchors(texts.get(target, target.read_text(encoding="utf-8"))):
                    self.fail(path, f"line {line_number}: missing anchor '#{anchor}' in {target.relative_to(self.docs_root)}")

    def validate_metadata_links(self, path: Path, metadata: dict[str, object]) -> None:
        for key in ("depends_on", "decisions"):
            values = metadata.get(key, [])
            if not isinstance(values, list):
                self.fail(path, f"'{key}' must be a YAML list")
                continue
            if key in metadata and not values:
                self.fail(path, f"empty '{key}' must be omitted")
            if len(values) != len(set(values)):
                self.fail(path, f"'{key}' contains duplicate targets")
            for value in values:
                target = (path.parent / value).resolve()
                if not target.is_file():
                    self.fail(path, f"'{key}' target does not exist: {value}")
                    continue
                try:
                    target.relative_to(self.docs_root)
                except ValueError:
                    self.fail(path, f"'{key}' target escapes Docs: {value}")
                if key == "decisions" and target.parent.name != "ADR":
                    self.fail(path, f"decision target is not in Docs/ADR: {value}")

        superseded_by = metadata.get("superseded_by")
        if superseded_by is not None:
            if metadata.get("status") != "superseded":
                self.fail(path, "'superseded_by' requires status 'superseded'")
            if not isinstance(superseded_by, str):
                self.fail(path, "'superseded_by' must be a relative path")
            else:
                target = (path.parent / superseded_by).resolve()
                if not target.is_file() or target.parent.name != "ADR":
                    self.fail(path, f"invalid 'superseded_by' ADR target: {superseded_by}")

    def validate_dependency_graph(self) -> None:
        graph: dict[Path, list[Path]] = {}
        for path, metadata in self.metadata.items():
            dependencies = metadata.get("depends_on", [])
            graph[path] = [
                (path.parent / value).resolve()
                for value in dependencies
                if isinstance(value, str) and (path.parent / value).resolve() in self.metadata
            ] if isinstance(dependencies, list) else []

        visiting: list[Path] = []
        visited: set[Path] = set()

        def visit(node: Path) -> None:
            if node in visiting:
                cycle = visiting[visiting.index(node):] + [node]
                rendered = " -> ".join(str(item.relative_to(self.docs_root)) for item in cycle)
                self.fail(node, f"depends_on cycle: {rendered}")
                return
            if node in visited:
                return
            visiting.append(node)
            for dependency in graph.get(node, []):
                visit(dependency)
            visiting.pop()
            visited.add(node)

        for path in sorted(graph):
            visit(path)

    def validate_adr_references(self) -> None:
        for path, metadata in self.metadata.items():
            decisions = metadata.get("decisions", [])
            if isinstance(decisions, list):
                for value in decisions:
                    if not isinstance(value, str):
                        continue
                    target = (path.parent / value).resolve()
                    target_metadata = self.metadata.get(target)
                    if target_metadata is not None and target_metadata.get("status") != "accepted":
                        self.fail(path, f"decision target is not accepted: {value}")

            superseded_by = metadata.get("superseded_by")
            if isinstance(superseded_by, str):
                target = (path.parent / superseded_by).resolve()
                target_metadata = self.metadata.get(target)
                if target_metadata is not None and target_metadata.get("status") not in {"accepted", "superseded"}:
                    self.fail(path, f"'superseded_by' target has invalid status: {superseded_by}")

    def run(self) -> int:
        markdown_files = sorted(self.docs_root.rglob("*.md"))
        texts: dict[Path, str] = {}
        for path in markdown_files:
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError as error:
                self.fail(path, f"not valid UTF-8: {error}")
                continue
            texts[path.resolve()] = text
            metadata = self.parse_front_matter(path, text)
            if metadata is None:
                continue
            self.metadata[path.resolve()] = metadata
            self.validate_front_matter(path, metadata)
            self.validate_metadata_links(path, metadata)
            self.validate_header(path, text)

        for path, text in texts.items():
            self.validate_links(path, text, texts)
        self.validate_adr_references()
        self.validate_dependency_graph()

        if self.errors:
            for error in sorted(set(self.errors)):
                print(f"ERROR: {error}", file=sys.stderr)
            print(f"Documentation validation failed with {len(set(self.errors))} error(s).", file=sys.stderr)
            return 1
        print(f"Documentation validation passed: {len(markdown_files)} Markdown files.")
        return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--docs-root",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "Docs",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="run positive and negative status/lifecycle cases",
    )
    arguments = parser.parse_args()
    if arguments.self_test:
        return run_self_test(arguments.docs_root)
    return Validation(arguments.docs_root).run()


def run_self_test(docs_root: Path) -> int:
    """Exercise status/location rules without creating fixture files."""

    common = {"title": "Fixture", "version": "1.0", "updated": "2026-08-20"}
    cases: list[tuple[str, str, dict[str, object], bool]] = [
        ("architecture", "Architecture/Fixture.md", {**common, "status": "normative"}, True),
        ("ui draft", "UI/Fixture.md", {**common, "status": "draft"}, True),
        ("root router", "README.md", {**common, "status": "informative"}, True),
        ("project brief", "ProjectBrief.md", {**common, "status": "informative"}, True),
        ("status report", "Status/Fixture.md", {**common, "status": "informative"}, True),
        ("active plan", "Plans/Work/Step.md", {**common, "status": "active"}, True),
        (
            "active proposal",
            "Proposals/Fixture.md",
            {**common, "status": "draft", "proposal_state": "accepted_for_planning"},
            True,
        ),
        ("accepted ADR", "ADR/0042-fixture.md", {"title": "ADR", "status": "accepted", "date": "2026-08-20"}, True),
        ("archived plan", "Plans/Archive/Fixture.md", {**common, "status": "archived"}, True),
        ("normative root", "Fixture.md", {**common, "status": "normative"}, False),
        ("normative plan", "Plans/Work/Step.md", {**common, "status": "normative"}, False),
        ("normative status", "Status/Fixture.md", {**common, "status": "normative"}, False),
        ("active architecture", "Architecture/Fixture.md", {**common, "status": "active"}, False),
        ("draft concept", "Concepts/Fixture.md", {**common, "status": "draft"}, False),
        ("informative contract", "Architecture/Fixture.md", {**common, "status": "informative"}, False),
        ("active archive", "Plans/Archive/Fixture.md", {**common, "status": "active"}, False),
    ]

    failures: list[str] = []
    root = docs_root.resolve()
    for name, relative, metadata, should_pass in cases:
        validation = Validation(root)
        validation.validate_front_matter(root / relative, metadata)
        passed = not validation.errors
        if passed != should_pass:
            failures.append(f"{name}: expected {'pass' if should_pass else 'failure'}")
    if failures:
        for failure in failures:
            print(f"ERROR: self-test {failure}", file=sys.stderr)
        return 1
    print(f"Documentation validator self-test passed: {len(cases)} cases.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
