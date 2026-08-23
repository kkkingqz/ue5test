---
title: CriticalCorrectiveHardening Archive Summary
status: archived
version: 1.0
updated: 2026-08-23
---

# CriticalCorrectiveHardening: итог выполнения

> **Материализует:** исторический итог выполненного плана; документ не является источником правил или задач.

## Цель и результат

**Цель:** исправить критические расхождения между принятым UI contract и фактической реализацией без расширения архитектуры и без нового функционала.

**Результат:** failed reconciliation/composite apply не оставляет частичное состояние; repeated content идентифицируется только стабильным explicit key; `CanApply*` полностью non-mutating; legacy single `Character`/`Meters[0]` fallback paths удалены; `ScalePolicy` — единственный источник graphics behavior, `RenderMode` — только проверка совместимости; verification измеряет фактическую arranged geometry, фактический font size и фактический resulting brush вместо желаемого размера и синтетических вызывающих.

## Этапы и задачи

### M1 — Repeater Atomicity and Identity

Failed reconciliation не мутирует live widgets; repeated entries имеют только stable explicit keys.

- `CCF-01` — Зафиксировать regression test partial mutation reused widget
- `CCF-02` — Сделать Repeater reconcile атомарным после preflight
- `CCF-03` — Сделать repeated key обязательным и удалить positional fallback
- `CCF-04` — Не использовать Character ResourceId как identity
- `CCF-05` — Закрыть Repeater regression matrix

### M2 — Location Composite Correctness

Pure preflight, отсутствие `[0]` fallback paths, atomic composite apply, корректные reset/placeholders.

- `CCF-06` — Сделать `CanApply*` полностью non-mutating
- `CCF-07` — Удалить legacy single `Character` rendering path
- `CCF-08` — Удалить legacy `StaminaMeter` / `Meters[0]` path
- `CCF-09` — Добавить regression tests на partial composite apply
- `CCF-10` — Разделить composite apply на pure preflight и commit
- `CCF-11` — Зафиксировать reset semantics для всех четырёх composites
- `CCF-12` — Зафиксировать существующую placeholder semantics

### M3 — Graphics Contract

`ScalePolicy` остаётся единственным runtime behavior source; assets и brush state соответствуют contract.

- `CCF-13` — Добавить regression test: `RenderMode` не меняет `ScalePolicy`
- `CCF-14` — Удалить `RenderMode → ScalePolicy` inference и поправить existing assets
- `CCF-15` — Закрыть graphics regression matrix на resulting brush

### M4 — Verification

Реальные geometry, consumer font, NineSlice и Tavern→Market tests.

- `CCF-16` — Проверять actual arranged geometry на шести viewport sizes
- `CCF-17` — Доказать фактическую пригодность 1280×720
- `CCF-18` — Доказать фактическое ultrawide allocation
- `CCF-19` — Проверить actual font size во всех существующих consumers
- `CCF-20` — Проверить NineSlice через реальный resulting brush
- `CCF-21` — Усилить Tavern → Market transition verification

### M5 — Closure

Документы больше не заявляют evidence, которого нет; corrective change set проходит полный project gate.

- `CCF-22` — Синхронизировать `UiFoundationHardening` и `LocationScreen`
- `CCF-23` — Выполнить targeted source audit
- `CCF-24` — Полный project gate и финальный отчёт

Итоговый DoD плана (21 пункт) синхронизирован задачей `SVC-12` плана [SceneAndVerificationCorrection](SceneAndVerificationCorrection.md) — все пять этапов были фактически завершены с evidence, но верхнеуровневый checklist оставался неотмеченным.

## Актуальные нормативные источники

- [UIDocumentAndReconciliation](../../UI/UIDocumentAndReconciliation.md)
- [ScreenTemplates](../../UI/ScreenTemplates.md)
- [ImageResources](../../UI/ImageResources.md)
- [WidgetRegistry](../../UI/WidgetRegistry.md)

## Полная история

`source_commit`: [f181970dab20fa615a388425f6018cac94e3435a](https://github.com/kkkingqz/ue5test/commit/f181970dab20fa615a388425f6018cac94e3435a)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/f181970dab20fa615a388425f6018cac94e3435a/Docs/Plans/CriticalCorrectiveHardening) содержит исходные task-файлы, acceptance criteria и evidence.
