---
title: Content Editor Session Safety and Performance Tasks
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - README.md
  - AuthoringIndexAndTreeBrowser.md
  - TypedReferencesAndRename.md
---

# M4 — Session Safety and Performance

> **Материализует:** Задачи CEH-18…22.
> **Результат:** navigation не теряет edits, external changes видимы заранее, а derived panels не перечитывают GameData на каждый input event.

## Задачи

- [ ] **CEH-18 — Единый navigation/selection gate**
  - Done: Browser selection, Reference navigation, Diagnostic navigation, provider switch, create/open operation и tab close не вызывают `LoadDefinition()` напрямую в обход session controller.
  - Если current definition dirty, обязателен `Save / Discard / Cancel`.
  - Cancel сохраняет текущую selection и pending edits; failed Save тоже сохраняет editor state.
  - Evidence: dirty-navigation Slate tests.

- [ ] **CEH-19 — External file watcher и stale-state UI**
  - Done: Editor использует UE file/directory watching mechanism; state различает Clean, Dirty, Stale, Dirty+Stale; stale виден до Save; Save поверх stale запрещён тем же typed contract, что backend.
  - Evidence: external write while definition is open.

- [ ] **CEH-20 — Recovery/Draft для Dirty + Stale**
  - Зависимости: CEH-19.
  - Done: пользователь может Cancel, Discard+Reload или сохранить pending edits как editor draft вне canonical GameData.
  - Draft содержит locator/base stamp/typed structural edits; import/reapply снова проходит validation.
  - Evidence: stale draft export + reapply fixture.

- [ ] **CEH-21 — Разделить edit events и expensive derived refresh**
  - Done: local edit немедленно обновляет pending value/dirty indicator; expensive reference/index/validation refresh выполняется по commit, debounce или targeted overlay; text typing не запускает full package read/parse на каждый символ; Browser tree не перестраивается при обычном data edit.
  - Evidence: instrumentation count while typing N chars.

- [ ] **CEH-22 — Не терять diagnostics и multi-file result metadata**
  - Done: initialization diagnostics отображаются после открытия tab; adapter result сохраняет `AffectedFilesCount`, `AffectedFilePaths`, `ReplacementsCount` и typed outcome details; UI не сводит multi-file rename к одному `AffectedFile`.
  - Evidence: init-failure fixture + multi-file rename result test.

## Проверка milestone

- [ ] Нельзя потерять edits простым click/navigation.
- [ ] Stale file виден без попытки Save.
- [ ] Dirty+Stale имеет recoverable workflow.
- [ ] N typed characters не означают N full GameData reparses.
- [ ] Init diagnostics видны.
- [ ] Multi-file operation result не теряет affected-file information.
