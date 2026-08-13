---
title: "ADR-0011: Blueprint Screen Templates"
status: accepted
date: 2026-08-10
---

# ADR-0011: Blueprint Screen Templates

## Context

В игре существует ограниченный набор визуально различающихся экранов. Их layout, animation, focus navigation и composition удобнее авторить в UMG, тогда как Lua обязан оставаться владельцем desired presentation data и не зависеть от Unreal Engine для headless simulation.

Универсальное построение физического Widget tree из Lua увеличивает объём schemas и reconciliation logic, переносит layout-решения в gameplay-код и затрудняет работу дизайнера. Отдельный C++ class/factory для каждого экрана, напротив, связывает presentation content с native module и требует пересборки C++ при добавлении экрана.

## Decision

- Каждый конкретный screen является cooked Widget Blueprint template, унаследованным от общего абстрактного `WBP_ScreenBase`/`UGV2ScreenWidgetBase`.
- Concrete Screen Blueprint задаёт layout и размещает поддерживаемые Dynamic Screen Elements. Каждый такой элемент объявляет `field_id`, `schema_id` и required/optional policy.
- Lua публикует только value-only `screen_id`, instance identity и полный набор Screen Fields. Blueprint class, UObject, raw asset path и callback boundary не пересекают.
- Screen Registry на UE-стороне сопоставляет `screen_id` с trusted cooked soft class и layer policy. C++ coordinator и base classes работают только с registry entry и schemas и не знают identifiers конкретных экранов.
- Новый экран обычно требует нового Widget Blueprint, Screen Registry entry и Lua model builder. Изменение C++ для нового `screen_id` запрещено, пока экран выражается существующими Screen Field schemas.
- Screen Field содержит presentation values и command bindings. Нажатие передаёт opaque UI binding handle; Blueprint не получает Lua function name и не является gameplay authority.
- Текущий vertical slice обязан адаптировать существующие `WBP_RichText` и `WBP_ButtonList`; расширение набора Dynamic Screen Elements принимается отдельным изменением по concrete need.
- Automation обязана использовать те же Session, Command/Semantic Input и Screen publication entry points, что runtime; test-only Screen factories/builders в C++, Blueprint и Lua façade запрещены.

Data-driven Screen Registry реализуется до фиксации окончательного C++ boundary, чтобы немедленно удалить concrete screen classes/paths из runtime flow. Стабилизация public C++ API остаётся отдельным решением; это не разрешает добавлять per-screen branches в production C++.

## Consequences

- Layout и visual composition остаются UE-authored, а desired values и command availability — Lua-authored.
- Lua runtime и balance tests могут создавать те же screen models без загрузки UMG или media.
- Базовый screen class централизует discovery, validation, atomic apply и lifecycle hook для динамических полей.
- Screen Blueprint имеет явный schema contract; несовпадение field/schema обнаруживается до interactive publication.
- Изменение layout без изменения field contract не требует Lua/C++ change. Breaking field contract требует schema evolution и синхронного обновления Lua producer.

## Rejected alternatives

- **Lua-authored universal Widget tree.** Отклонено: Lua начинал бы владеть layout/composition и физической структурой presentation.
- **Native C++ class для каждого screen.** Отклонено: добавление presentation content требовало бы изменения и пересборки native module.
- **Blueprint class/path из Lua.** Отклонено: нарушает value-only boundary, headless portability и resource indirection.
- **Lua callback на Dynamic Screen Element.** Отклонено: C++ не хранит Lua callbacks; interaction проходит через binding и Command Dispatcher.
