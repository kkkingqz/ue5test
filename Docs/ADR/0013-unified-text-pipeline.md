---
title: "ADR-0013: Unified Data-Driven Text Pipeline"
status: accepted
date: 2026-08-12
---

# ADR-0013: Unified Data-Driven Text Pipeline

> **Решение:** Единый TextSpec pipeline и data-driven typography/markup tokens.
> **Нормативный текст:** [Widget Registry](../UI/WidgetRegistry.md), [UI Index](../UI/README.md).

## Context

Plain CommonUI text, RichText, button labels, tooltip text и custom Slate runs применяли typography разными путями. Это привело к расхождению font fallback: интерактивный фрагмент не наследовал composite font окружающего текста и некорректно отображал кириллицу. Одновременно localized content должен уметь задавать семантическое форматирование без UE asset paths и физических RGB/font-size значений.

## Decision

- Любой runtime-authored display text пересекает portable boundary как `TextSpec`: `text_id`, typed `args` и optional local `style` token.
- UE `Text Pipeline` является единственной точкой localization resolution, argument escaping/formatting, markup validation и typography resolution.
- Active `UGV2UiTheme` содержит data-driven maps style/color/size tokens. Добавление значения token не требует изменения C++.
- Localized markup использует ограниченные структурные теги `br`, `color`, `size`, `style`, `interactive`; произвольные UE decorators, asset paths, RGB и числовые font sizes запрещены.
- Parser нормализует вложенный authoring markup в flat render runs. Interactive run наследует результирующую typography и добавляет только interaction state.
- Headless runtime сохраняет unresolved `TextSpec`; gameplay semantics не зависят от locale или rendering.

`style` является presentation token, а не asset locator и не gameplay authority. Этим решением уточняется отклонённая в ADR-0012 передача физических styles из Lua: передача UE style class/path остаётся запрещённой.

## Consequences

- Plain text, RichText, buttons и tooltip используют одинаковые localization и font rules.
- Theme обязан покрывать glyph ranges поддерживаемых locales composite fonts.
- Missing `text_id`, token, malformed tag или unsafe argument являются validation error до Widget mutation.
- Culture/theme change инвалидирует resolved presentation cache, но не Lua state.

## Rejected alternatives

- **Прямой UE RichText markup.** Не поддерживает требуемое стабильное вложение и раскрывает engine-specific grammar контенту.
- **Font/size/RGB в локализованной строке.** Создаёт theme drift и platform-dependent rendering.
- **Разрешать localized text в Lua.** Нарушает headless separation и дублирует platform localization semantics.
