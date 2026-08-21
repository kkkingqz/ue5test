---
title: Presentation Pipeline Tasks
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - README.md
  - ../../UI/ImageResources.md
  - ../../UI/ScreenTemplates.md
---

# M2 — Presentation Pipelines

> **Материализует:** разделы 5—7 [предложения](../../Proposals/CoreUIBaselineAndScalingProposal.md).
> **Задачи:** UIH-05…08.
> **Результат:** заявленные text/graphics contracts работают в фактическом
> rendering path, а не только существуют как вспомогательная модель.

## Задачи

- [ ] **UIH-05 — Подключить DPI scaling к центральному text pipeline**
  - Done: effective text size вычисляется общим resolver через theme,
    semantic size/style token и фактическую viewport height; plain Text,
    Button, InputField, Dropdown и другие Core consumers не имеют локальной
    формулы масштаба; `MinReadableFontSize` применяется в реальном renderer.
  - Constraint: gameplay composite не знает physical font size.
  - Evidence: `GV2TextPipeline.*`, `GV2UiTheme.*`, consumer tests.

- [ ] **UIH-06 — Унифицировать scaling plain и rich text**
  - Зависимости: UIH-05.
  - Done: default rich text style и `<size=...>` runs используют тот же
    effective-size resolver, что plain text; raw `TextSizeTokens` не
    применяются к rich run как unscaled physical size; одинаковый semantic
    token даёт одинаковый effective size независимо от renderer.
  - Negative: semantic run не может обойти minimum readable size.
  - Evidence: `GV2RichTextWidgetBase.*`,
    `GV2RichTextSpanDecorator.*`, text pipeline automation.

- [ ] **UIH-07 — Убрать `AcceptedRenderMode` как источник поведения**
  - Done: runtime behavior изображения задаётся только `ScalePolicy`;
    `RenderMode` приходит из resolved resource metadata и используется только
    в `IsScalePolicyCompatible`; код не преобразует `PreserveAspect` в `Tile`
    или `NineSlice` на основании второго свойства.
  - Migration: старое serialized property допускается временно как
    `*_DEPRECATED` только для миграции `.uasset`, но runtime его не читает.
  - Evidence: `GV2ImageWidgetBase.*`, `GV2ImagePresentation.*`,
    affected `.uasset`, negative compatibility tests.

- [ ] **UIH-08 — Оставить один graphics apply path**
  - Зависимости: UIH-07.
  - Done: устаревший overload `ResolveAndApply(...AcceptedRenderMode...)`
    удалён или является недоступным compatibility shim без runtime consumers;
    required и optional resources проходят через один policy-based resolver;
    incompatible resource отклоняется до `SetBrush`.
  - Test: тот же `resource_id + ScalePolicy` даёт одинаковый результат во всех
    consumers; invalid combination не меняет предыдущий brush.
  - Evidence: `GV2ImagePresentation.*`,
    `GV2ImageWidgetBase.*`, graphics automation.

## Проверка milestone

- [ ] Реальный text renderer вызывает DPI-aware sizing.
- [ ] Plain и RichText используют одинаковый resolver.
- [ ] Minimum readable size проверяется на rendered widgets.
- [ ] `ScalePolicy` — единственный runtime source of behavior.
- [ ] Resource render mode используется только как compatibility gate.
- [ ] Failed graphics validation не мутирует widget.
