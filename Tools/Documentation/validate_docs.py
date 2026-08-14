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
            else {"draft", "normative", "deprecated"}
        )
        if status and status not in allowed_status:
            self.fail(path, f"invalid status '{status}'")

        version = metadata.get("version")
        if version and (not isinstance(version, str) or VERSION.fullmatch(version) is None):
            self.fail(path, f"version '{version}' must have major.minor form")

        date_value = metadata.get("date" if is_adr else "updated")
        if isinstance(date_value, str):
            try:
                dt.date.fromisoformat(date_value)
            except ValueError:
                self.fail(path, f"invalid ISO date '{date_value}'")

        allowed_proposal_states = {"accepted_for_planning", "measurement_required", "implemented"}
        proposal_state = metadata.get("proposal_state")
        if is_proposal and proposal_state not in allowed_proposal_states:
            self.fail(path, f"invalid proposal_state '{proposal_state}'")

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
    arguments = parser.parse_args()
    return Validation(arguments.docs_root).run()


if __name__ == "__main__":
    raise SystemExit(main())
