---
title: "ADR-0012: Centralized UI Theme"
status: accepted
date: 2026-08-11
---

# ADR-0012: Centralized UI Theme

> **Решение:** Один UE-side theme asset и обязательное central styling reusable UI components.
> **Нормативный текст:** [Widget Registry](../UI/WidgetRegistry.md).

## Context

Reusable Widget Blueprints уже ссылались на отдельные CommonUI style classes. При локальном назначении style в каждом Widget изменение визуального языка требует обхода множества assets и допускает незаметный drift. Новые нейтральные UI primitives также используют разные типы Slate/CommonUI style values, поэтому одного CommonTextStyle недостаточно.

## Decision

- UE presentation использует один настроенный `UGV2UiTheme` Data Asset как source of truth default styles UI-kit.
- `UGV2UiThemeSettings.ThemeAsset` является единственным project-level locator active theme. Locator не пересекает Lua boundary.
- Все reusable UI components реализуют `IGV2UiStyleConsumer` и применяют active theme при `NativePreConstruct`.
- Theme содержит CommonUI text/button style classes и Slate values для interactive RichText, popover surface, image tint, list spacing, progress, separator и loading indicator.
- Concrete Screen Blueprint может задавать layout, но не обязан копировать default visual tokens в каждый child Widget.
- Style не меняет gameplay-state, Screen Field semantics, `command_id`, binding или `resource_id`.
- Lua может передать local semantic `style` token по правилам [ADR-0013](0013-unified-text-pipeline.md), но не UE style class/path или физические typography values.

## Consequences

- Default visual language изменяется в одном Data Asset и связанных CommonUI style classes.
- Widget Blueprint остаётся пригодным для preview и автоматически получает current theme при reconstruction.
- Runtime theme replacement требует reapply/reconstruction UI, но не restart Lua gameplay и не изменение save.
- Новый UI component обязан либо реализовать central style consumer, либо быть явно документированным purely structural container без собственного visual style.

## Rejected alternatives

- **Style только в каждом Widget Blueprint.** Отклонено из-за дублирования и drift.
- **Передача style из Lua.** Отклонено: visual theme принадлежит UE presentation и не является gameplay data.
- **Один общий native base для всех Widgets.** Отклонено: primitives наследуются от разных UMG/CommonUI bases, включая `UCommonButtonBase`.
