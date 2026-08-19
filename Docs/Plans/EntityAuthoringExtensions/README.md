---
title: Entity Authoring Extensions Implementation Plan
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - ../../Proposals/EntityAuthoringExtensionProposal.md
  - ../../Architecture/LuaRuntimeContract.md
  - ../../Architecture/Modding.md
decisions:
  - ../../ADR/0027-designer-lua-authoring-layer.md
  - ../../ADR/0028-simplified-authoring-surface.md
  - ../../ADR/0030-textsystem-layer-and-data-driven-package-set.md
  - ../../ADR/0031-entity-authoring-extensions.md
---

# План реализации Entity Authoring Extensions

> **Материализует:** [Entity Authoring Extension Proposal](../../Proposals/EntityAuthoringExtensionProposal.md) и [ADR-0031](../../ADR/0031-entity-authoring-extensions.md).
> **Задачи:** EAE-01…12.
> **Результат:** декларативное добавление методов к сущностям через синтаксис `function EntityKind:method()`, автоматическая композиция effective method tables, устранение низкоуровневых декораторов из геймплейных пакетов.

## Цель

Предоставить авторам контента и модов единый предметный синтаксис добавления методов к доменным сущностям (`Actor`, `Location`, `Quest`, `Item`) в authoring-скриптах, полностью изолировав геймплейный код от runtime-механики регистрации типов, декораторов, `setmetatable` и метатабличных цепочек.

## Состояние на входе

| Что | Сейчас |
|---|---|
| Добавление методов к Actor | Низкоуровневые модули `scripts/gameplay/actors.lua` с ручными `register_type("player", decorator)` и `setmetatable` |
| Покрытие типов акторов | Дублирование привязки методов для каждого дискриминатора (`player`, `npc`) |
| Расширение Location | Декораторы определений через `properties.register_definition_type("location", decorator)` |
| Разрешение конфликтов методов | Отсутствует: последний зарегистрированный декоратор оборачивает предыдущий без проверки коллизий имён методов |
| Синтаксис авторских методов | Низкоуровневый процедурный код вместо декларативного `function Actor:method()` |

## Milestones

- [x] **M1 — [Extension Registry](ExtensionRegistry.md)**: централизованный реестр расширений сущностей (`core:module.runtime.entity_extension_registry`), детерминированная композиция `effective method table`, проверка конфликтов и дубликатов на этапе инициализации.
- [x] **M2 — [Authoring Prototypes](AuthoringPrototypes.md)**: инжекция контролируемых прокси-прототипов (`Actor`, `Location`, `Quest`, `Item`) в `_ENV` authoring-скриптов, сбор деклараций через `__newindex`, контекст пакета для `fail()`, интеграция с валидацией managed properties (DLA-12).
- [x] **M3 — [Actor Migration](ActorMigration.md)**: миграция методов `Actor` в `textsystem` (`is_player`, `is_npc`, `require_location`, `move_to`) и `rh` (`get_gold`, `add_gold`, `require_gold`, `spend_gold`, `add_item`), удаление низкоуровневых декораторов из игровых пакетов.
- [ ] **M4 — [Definition Extension and Validation](DefinitionExtensionAndValidation.md)**: расширение определений (`Location`), контракт `self` для definition vs instance, кросс-слойная изоляция и спеки для всех уровней тестирования (`Core`, `TextSystem`, `FullGame`).

## Критический путь

```text
M1 (Registry & Composition) ──► M2 (Authoring Prototypes) ──► M3 (Actor Migration) ──► M4 (Definitions & Specs)
```

## Общие правила выполнения

1. Никакие runtime-функции или метатаблицы не попадают в save container ([INV-001](../../Architecture/Invariants.md), [INV-008](../../Architecture/Invariants.md)).
2. Мутация канонического состояния внутри методов сущностей разрешена только во время исполнения команд в открытом окне мутации ([INV-003](../../Architecture/Invariants.md)).
3. Дублирование метода между независимыми пакетами — фатальная ошибка на этапе bootstrap ([INV-010](../../Architecture/Invariants.md)).
4. Сохраняется строгая 3-слойная иерархия: `core` $\leftarrow$ `textsystem` $\leftarrow$ `rh` ([ADR-0030](../../ADR/0030-textsystem-layer-and-data-driven-package-set.md)).
5. Поведение и наблюдаемые результаты существующих спек остаются неизменными.

## Итоговый Definition of Done

- [ ] Методы сущностей объявляются через обычный синтаксис `function EntityKind:method_name(...)`.
- [ ] Из `textsystem` и `rh` полностью удалены ручные вызовы `register_type`, декораторы и `setmetatable`.
- [ ] Конфликты методов между пакетами обнаруживаются до начала сессии с информативной ошибкой `entity_extension.method_conflict`.
- [ ] Экземпляры сущностей получают методы через скомпонованную `effective method table`.
- [ ] Вызовы `fail()` из методов сущностей детерминированно атрибутируются пространством имён пакета объявления.
- [ ] Спеки всех трёх уровней (`Core`, `TextSystem`, `FullGame`) проходят успешно на обоих хостах (UE и Headless).
