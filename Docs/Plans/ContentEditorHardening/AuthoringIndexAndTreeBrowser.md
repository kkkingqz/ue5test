---
title: Content Editor Authoring Index and Tree Browser Tasks
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - README.md
  - ../Archive/ContentEditor.md
  - ../../Architecture/GameDataRepositoryContract.md
  - ../../Architecture/StableIDSpecification.md
---

# M1 — Authoring Index and Tree Browser

> **Материализует:** Задачи CEH-01…05.
> **Результат:** Editor имеет provider-aware read model и древовидный browser, построенный напрямую из canonical Stable IDs.

## Задачи

- [x] **CEH-01 — Ввести portable Authoring Locator и Authoring Index**
  - Done: authoring read surface различает canonical Stable ID, provider package ID, package-relative source, definition type/kind, effective winner/shadowed state и source-level authoring locator.
  - Locator не зависит от Slate/Unreal UObject.
  - Authoring Index строится из canonical package discovery/repository/provenance semantics, а не повторяет overlay rules внутри `FGV2EditorAdapter`.
  - Один Stable ID может иметь несколько physical provider entries, но ровно один effective winner при valid resolved repository.
  - Negative: invalid provider set не превращается в произвольный "первый найденный" selection.
  - Evidence: portable DTO/index implementation + core/override conformance fixture.

- [x] **CEH-02 — Перевести `FGV2EditorAdapter` на provider-aware selection**
  - Зависимости: CEH-01.
  - Done: `LoadDefinition`, duplicate/delete/rename/navigation не выбирают physical source только по первому `IndexedDefinitions` match.
  - Current selection хранит authoring locator.
  - `FindPackageRootForDefinition(id)` не является hidden provider resolver.
  - Evidence: adapter tests с одинаковым ID в core + override package.

- [x] **CEH-03 — Заменить плоский browser на Stable-ID `STreeView`**
  - Зависимости: CEH-01.
  - Done: Browser использует canonical `FStableId::Parse()` и строит `namespace -> kind -> path segment -> ...`.
  - Tree nodes являются presentation-only и ничего не записывают в JSON5.
  - Узел может одновременно иметь `Definition` payload и `Children`.
  - Evidence: tree-model tests + Slate tree widget.

- [x] **CEH-04 — Search, expansion state и dirty indication**
  - Зависимости: CEH-03.
  - Done: filter сохраняет matching definition и ancestors; ancestors раскрываются во время search; очистка search восстанавливает пользовательское expansion state; dirty definition и collapsed dirty ancestor имеют indicator.
  - Evidence: deterministic tree/filter tests + Slate interaction test.

- [x] **CEH-05 — Effective view и provider inspection**
  - Зависимости: CEH-01…04.
  - Done: default Browser mode показывает effective definitions без duplicate rows одного Stable ID; overridden definition показывает winning provider badge; provider inspection показывает winner/shadowed entries и может открыть конкретный authoring locator.
  - Provider presentation не меняет Stable-ID hierarchy.
  - Evidence: core + mod override integration fixture.

## Проверка milestone

- [x] Stable-ID tree работает без persisted folder metadata.
- [x] Effective view не содержит duplicate Stable ID rows.
- [x] Physical provider selection не теряется при navigation.
- [x] Search и expansion работают детерминированно.
- [x] Override fixture открывает именно выбранный provider.
