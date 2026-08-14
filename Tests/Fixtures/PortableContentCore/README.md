# PortableContentCore shared fixtures

This is the single fixture corpus for CMake/standalone and Unreal automation tests.
Hosts resolve this directory from the repository/project root; fixtures are never
copied into a host-specific test tree.

`fixtures.index` is a sorted, path-only inventory. Blank lines and lines starting
with `#` are ignored. It is test-harness metadata, not a content package manifest
or a `GV2ContentCore` input grammar.

Current cases:

- `valid/empty_core` represents a core descriptor with an empty source list for M1;
- `valid/core` is the executable M3 representative package for exactly five kinds:
  `location`, `screen`, `item`, `text`, `resource`;
- `valid/test_mod` adds its own Screen, fully overrides core Screen/Location,
  declares a two-hop redirect chain and a tombstone;
- `invalid/active_redirect_source` covers active definition versus redirect/tombstone conflict;
- `invalid/duplicate_key` is a parser failure fixture;
- `invalid/broken_override` is a schema/full-override failure fixture.
- `invalid/foreign_new_id` violates package namespace ownership;
- `invalid/missing_reference` points at an absent winner;
- `invalid/resource_class_mismatch` resolves a resource of the wrong declared class;
- `invalid/redirect_cycle` contains a manifest-driven redirect cycle test package;
- `invalid/semantic_failure` reaches the built-in item semantic validator.

M2 parser conformance ожидает `core:diagnostic.json5.duplicate_key` только для
`invalid/duplicate_key`; остальные документы обязаны быть syntactically valid.
`GV2ContentCore::Testing::MakeRepresentativeCorePackageDescriptor()` binds the
ten core sources for both hosts. Completed M4 produces a publishable
`RepositoryResolved` snapshot: core has nine definitions; core + `valid/test_mod`
has eleven canonical winners, complete replacements, flattened redirects, provenance,
minimal indexes and a deterministic content hash.
