---
title: EntityAuthoringExtensions Archive Summary
status: archived
version: 1.0
updated: 2026-08-19
---

# EntityAuthoringExtensions: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Предоставить авторам контента и модов единый предметный синтаксис добавления методов к доменным сущностям (`Actor`, `Location`, `Quest`, `Item`) в authoring-скриптах, полностью изолировав геймплейный код от runtime-механики регистрации типов, декораторов, `setmetatable` и метатабличных цепочек

**Результат:** декларативное добавление методов к сущностям через синтаксис `function EntityKind:method()`, автоматическая композиция effective method tables, устранение низкоуровневых декораторов из геймплейных пакетов

## Этапы и задачи

### M1 — Extension Registry

`core:module.runtime.entity_extension_registry` собирает методы, валидирует конфликты и формирует неизменяемые effective method tables

- `EAE-01` — ADR-0031: Архитектура расширения сущностей через авторские прототипы
- `EAE-02` — Модуль реестра расширений сущностей (`core:module.runtime.entity_extension_registry`)
- `EAE-03` — Валидация конфликтов и дубликатов методов
- `EAE-04` — Построение и кэширование `effective method table`

### M2 — Authoring Prototypes and Environment

в `_ENV` authoring-скриптов доступны `Actor`, `Location`, `Quest`, `Item`; методы объявляются синтаксисом `function EntityKind:method(...)`; `fail()` сохраняет контекст пакета объявления; managed-свойства валидируются по `effective method table`

- `EAE-05` — Контролируемые прокси-прототипы в `_ENV` (`authoring_context.lua`)
- `EAE-06` — Атрибуция пакета и контекст `fail()` в методах сущностей
- `EAE-07` — Интеграция с валидацией Managed Properties (DLA-12)

### M3 — Actor Migration

`textsystem` и `rh` объявляют методы `Actor` через `function Actor:method()`; ручные декораторы и `register_type` удалены

- `EAE-08` — Миграция методов Actor в `textsystem`
- `EAE-09` — Миграция ресурсных и предметных методов Actor в `rh`
- `EAE-10` — Очистка `actor_registry.lua` и унификация доступа к экземплярам

### M4 — Definition Extension and Multi-Tier Validation

расширение `Location` через `function Location:method()`, контракт `self` для определений контента, сквозное покрытие тестами на трёх уровнях (`Core`, `TextSystem`, `FullGame`), проверка детекции конфликтов

- `EAE-11` — Расширение Location через прототип в authoring-скриптах
- `EAE-12` — Спеки валидации конфликтов, композиции и изоляции уровней

## Актуальные нормативные источники

- [AuthoringSurfaceContract](../../Architecture/AuthoringSurfaceContract.md)
- [CanonicalStateAndSave](../../Architecture/CanonicalStateAndSave.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/EntityAuthoringExtensions) содержит исходные task-файлы, acceptance criteria и evidence.
