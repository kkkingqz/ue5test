---
title: Content Editor Typed References and Rename Tasks
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - README.md
  - AuthoringIndexAndTreeBrowser.md
  - SchemaDrivenPropertyEditing.md
  - ../../Architecture/GameDataRepositoryContract.md
---

# M3 — Typed References and Rename

> **Материализует:** Задачи CEH-13…17.
> **Результат:** References/Delete/Rename/Pickers используют schema-typed authoring index, а не эвристический поиск строк.

## Задачи

- [x] **CEH-13 — Построить typed authoring reference index**
  - Зависимости: CEH-01.
  - Done: tooling-derived index фиксирует только реальные `ref`, `text_id`, `resource_ref` и extension-schema reference sites.
  - Entry хранит source locator, source definition ID, JSON pointer/span, reference kind, expected target kind/resource class и target ID.
  - Ordinary string, даже если он синтаксически выглядит как Stable ID, не становится reference.
  - Incoming view строится как derived reverse map; runtime repository contract не получает новый обязательный reverse index.
  - Evidence: false-positive negative fixture.

- [x] **CEH-14 — Перевести References, Delete и Pickers на единый typed index**
  - Зависимости: CEH-13.
  - Done: `Uses`/`Used by`, Delete blocker, reference/resource/text pickers используют один typed inspection source.
  - `GetCompatibleResourceTargets` не перечитывает все resource files при каждом control creation.
  - Старый recursive string scanner удалён или не имеет consumers.
  - Evidence: read surface + delete/picker integration tests.

- [x] **CEH-15 — Отражать pending edits в reference views**
  - Зависимости: CEH-08, CEH-13.
  - Done: Outgoing references текущей definition строятся по effective pending candidate, а не только load-time baseline; Incoming index заменяет on-disk contribution текущего edited source на in-memory overlay без full-dataset reparse.
  - Discard восстанавливает baseline references.
  - Evidence: edit reference -> view changes -> discard/save round-trip.

- [x] **CEH-16 — Ввести Rename Impact model**
  - Зависимости: CEH-01, CEH-13.
  - Done: до rename UI показывает выбранный provider, own-package typed references, external-package typed references, redirect/tombstone conflicts и количество файлов/замен.
  - External references никогда не переписываются молча.
  - Evidence: core definition referenced from gameplay/mod package.

- [x] **CEH-17 — Schema-aware rename, redirect policy и transaction safety**
  - Зависимости: CEH-16.
  - Done: identity меняется в выбранном physical provider; own-package references переписываются только в typed sites; unrelated strings не меняются; external packages не мутируются автоматически; safe flow умеет создать redirect, если это разрешено; multi-file operation имеет crash-recovery mechanism либо эквивалентный transaction contract.
  - Existing controlled rollback не считается достаточным crash contract.
  - Evidence: failure injection между file commits + restart/recovery test.

## Проверка milestone

- [x] Stable-ID-looking ordinary strings не считаются references.
- [x] Delete не блокируется ложной строковой ссылкой.
- [x] Dirty reference edit виден до Save.
- [x] Resource picker не выполняет per-open full parse.
- [x] Rename показывает external impact.
- [x] Rename не меняет unrelated strings.
- [x] Failure injection не оставляет silently half-renamed content set.
