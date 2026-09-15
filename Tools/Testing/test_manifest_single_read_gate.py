#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).with_name("validate_manifest_single_read.py")


def load_gate():
    spec = importlib.util.spec_from_file_location("validate_manifest_single_read", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {SCRIPT_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ManifestSingleReadGateTests(unittest.TestCase):
    def test_accepts_one_blessed_manifest_read(self) -> None:
        gate = load_gate()
        source = """
std::optional<std::string> ReadManifestFileToString(const std::filesystem::path& FilePath)
{
    return ReadFileToString(FilePath);
}

std::optional<FArtifact> DiscoverPackageArtifactFromDirectory(const std::filesystem::path& Root)
{
    const auto PackageDescriptorPath = Root / "package.json5";
    const auto ManifestContent = ReadManifestFileToString(PackageDescriptorPath);
    return FArtifact{};
}
"""
        self.assertEqual([], gate.find_violations(source))

    def test_rejects_second_wrapper_call(self) -> None:
        gate = load_gate()
        source = """
std::optional<std::string> ReadManifestFileToString(const std::filesystem::path& FilePath)
{
    return ReadFileToString(FilePath);
}

std::optional<FArtifact> DiscoverPackageArtifactFromDirectory(const std::filesystem::path& Root)
{
    const auto PackageDescriptorPath = Root / "package.json5";
    const auto First = ReadManifestFileToString(PackageDescriptorPath);
    const auto Second = ReadManifestFileToString(PackageDescriptorPath);
    return FArtifact{};
}
"""
        self.assertTrue(any("exactly one manifest read call" in error for error in gate.find_violations(source)))

    def test_rejects_direct_manifest_read_bypass(self) -> None:
        gate = load_gate()
        source = """
std::optional<std::string> ReadManifestFileToString(const std::filesystem::path& FilePath)
{
    return ReadFileToString(FilePath);
}

std::optional<FArtifact> DiscoverPackageArtifactFromDirectory(const std::filesystem::path& Root)
{
    const auto PackageDescriptorPath = Root / "package.json5";
    const auto ManifestContent = ReadManifestFileToString(PackageDescriptorPath);
    return FArtifact{};
}

std::optional<FRoots> ReadRootsAgain(const std::filesystem::path& Root)
{
    return ReadFileToString(Root / "package.json5");
}
"""
        self.assertTrue(any("direct package.json5 read bypass" in error for error in gate.find_violations(source)))


if __name__ == "__main__":
    unittest.main()
