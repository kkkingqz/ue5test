---
title: Portable Content Core JSON5 Parsing Tasks
status: archived
version: 0.2
updated: 2026-08-13
depends_on:
  - Foundation.md
  - ../../../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# M2 — JSON5 Parsing

## Результат этапа

Portable parser преобразует bounded UTF-8 JSON5 source в value tree с точными source spans либо возвращает deterministic ordered diagnostics. Parsed source model существует только во время candidate build.

## Задачи

- [x] **PCC-07 — Реализовать UTF-8 input и parser limits**
  - Зависимости: PCC-03, PCC-04.
  - Проверять UTF-8/BOM и configurable limits для bytes, nesting depth, string length и container entries.
  - Done: каждый limit имеет stable diagnostic; ошибка прекращает только текущий candidate build без crash/hang.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/ParseLimits.h`, `Source/GV2ContentCore/Private/ParseLimits.cpp`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.ParseLimits` (Passed).

- [x] **PCC-08 — Реализовать JSON5 lexer**
  - Зависимости: PCC-07.
  - Поддержать comments, quoted/unquoted permitted keys, strings, numbers и trailing commas в пределах принятой grammar.
  - Done: каждый token имеет byte offsets и one-based line/column; invalid token возвращает точный span.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/Json5Lexer.h`, `Source/GV2ContentCore/Private/Json5Lexer.cpp`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.Json5Lexer` (Passed).

- [x] **PCC-09 — Реализовать JSON5 parser**
  - Зависимости: PCC-08.
  - Построить portable parsed tree без Unreal JSON и без formatting-preserving AST.
  - Done: scalar roots, arrays и objects разбираются; malformed/truncated input выдаёт bounded diagnostic (`core:diagnostic.json5.unexpected_eof`, `core:diagnostic.json5.syntax_error`). Transient `FParsedDocument` сохраняет JSON Pointer, value span и key span.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/Json5Parser.h`, `Source/GV2ContentCore/Private/Json5Parser.cpp`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.Json5Parser` (Passed).

- [x] **PCC-10 — Добавить duplicate-key detection**
  - Зависимости: PCC-09.
  - Duplicate object key является fatal независимо от равенства значений.
  - Done: diagnostic указывает повторное объявление и сохраняет related span первого объявления (`core:diagnostic.json5.duplicate_key`).
  - Evidence: `Source/GV2ContentCore/Private/Json5Parser.cpp`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.Json5Parser` (Passed).

- [x] **PCC-11 — Реализовать numeric normalization**
  - Зависимости: PCC-09.
  - Различать int64 и double; отклонять overflow, NaN и infinity; canonicalize `-0.0` в `+0.0`.
  - Done: raw spelling числа не влияет на normalized representation/hash при одинаковом значении и типе (`core:diagnostic.json5.invalid_number`).
  - Evidence: `Source/GV2ContentCore/Private/Json5Parser.cpp`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.Json5Parser` (Passed).

- [x] **PCC-12 — Добавить parser conformance suite**
  - Зависимости: PCC-07–PCC-11.
  - Включить документационные examples, Unicode/Cyrillic, comments, trailing commas, duplicate keys и limits.
  - Done: file permutation и повторный запуск дают одинаковый tree либо одинаковый diagnostic set.
  - Evidence: portable `GV2ContentCore::Testing::RunJson5FixtureConformance`, вызываемый `Headless/Source/main.cpp` и `GV2ContentCoreSharedFixtures`; общий `Tests/Fixtures/PortableContentCore/fixtures.index` выполняется в прямом и обратном порядке.

## Проверка milestone

- [x] Все обязательные JSON5 fixtures выполняются одним portable test suite.
- [x] Source spans совпадают в CMake и UBT tests.
- [x] Parser не выполняет schema validation, package resolution или I/O за пределами переданного source provider.
- [x] Parse representation освобождается после завершения build/freeze.
