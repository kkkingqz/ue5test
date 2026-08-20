---
title: Architecture Index
status: normative
version: 1.5
updated: 2026-08-20
depends_on:
  - ../README.md
---

# Архитектурные контракты

Раздел нормативный: он определяет, как GV2 обязан работать. Объяснения живут в [Concepts](../Concepts/README.md), programmer-инструкции — в [Guides](../Guides/README.md), designer-facing Lua reference — в [Authoring](../Authoring/README.md), причины выбора — в [ADR](../ADR/README.md).

## Навигация

| Документ | Когда читать |
|---|---|
| [Overview](Overview.md) | Источники истины, слои, границы C++, non-goals |
| [Dependency Map](DependencyMap.md) | Кто от кого может зависеть и что запрещено |
| [Invariants](Invariants.md) | Стабильные ID инвариантов и где их нормативный текст |
| [Glossary and Naming](GlossaryAndNaming.md) | Термины, имена, категории ID |
| [System Context and Components](SystemContextAndComponents.md) | Модули, ownership, lifetime |
| [Bootstrap and Session Lifecycle](BootstrapAndSessionLifecycle.md) | Cold start, session, restart, teardown |
| [Lua Runtime Contract](LuaRuntimeContract.md) | VM, модули, protected execution, value boundary |
| [Runtime Facade and Registries](RuntimeFacadeAndRegistries.md) | Карта `game`, общий registry lifecycle и freeze gate |
| [Lua Authoring Surface](AuthoringSurfaceContract.md) | Designer-facing `_ENV`, declarations и runtime adapters |
| [Commands and Events](CommandsAndEvents.md) | Команды, валидаторы, mutation window, события, фазы |
| [Canonical State and Save](CanonicalStateAndSave.md) | Форма состояния, инварианты экземпляров, сейв |
| [GameDataRepository Contract](GameDataRepositoryContract.md) | Сборка репозитория, override, снимок, чтение |
| [Definition Envelope and Schema Rules](DefinitionEnvelopeAndSchemaRules.md) | Конверт файла, схемы, значения по умолчанию |
| [Stable ID Specification](StableIDSpecification.md) | Грамматика, namespace, redirects, жизненный цикл |
| [Compatibility Policy](CompatibilityPolicy.md) | Project version и гарантии по save, schemas, API и Stable ID |
| [Headless Simulation Contract](HeadlessSimulationContract.md) | Роли UE-free хоста, conformance, manifest и digest |
| [Build and Tooling](BuildAndTooling.md) | Таргеты, хосты, CLI, фикстуры, CI |
| [Modding](Modding.md) | Пакеты модов, границы доверия |

## Authority, lifecycle и header

`status: normative` допустим только для contracts в `Architecture/` и `UI/`; ADR используют decision statuses. Активные Plans имеют `active`, архивы — `archived`, Concepts, Guides, Authoring, Status, Project Brief и ненормативные routers — `informative`. Активные Proposals сохраняют `draft` и отдельный `proposal_state`. Валидатор проверяет status в обе стороны.

После заголовка документ открывает blockquote с первым полем из таблицы. Индексы и исторические records header не требуют.

| Раздел | Обязательное поле | Остальные поля |
|---|---|---|
| `Architecture/`, `UI/` | **Владеет** | Не владеет, Инварианты, Реализация, Проверки |
| `Concepts/` | **Объясняет** | Нормативно, Не является нормативным |
| `Guides/` | **Задача** | Нужно, Нормативно |
| `Authoring/` | **Помогает** | Нормативно, Источник примера, Типичные ошибки |
| `ADR/` | **Решение** | Нормативный текст |
| `Proposals/` | **Предлагает** | Затрагивает, Не является нормативным (или Состояние, если реализовано) |
| `Plans/` | **Материализует** | Задачи, Результат |
| `Status/` | **Показывает** | Не является нормативным, Обновляется |

Пример для contract:

```markdown
> **Владеет:** конвертом команды, порядком валидаторов, mutation window, публикацией событий.
> **Не владеет:** конкретными командами и правилами игры.
> **Инварианты:** [INV-003](Invariants.md), [INV-004](Invariants.md)
> **Реализация:** `Scripts/runtime/command_dispatcher.lua`, `event_bus.lua`.
> **Проверки:** `Tests/Lua/commands/`, `Tests/Lua/events/`.
```

Header не повторяет front matter или назначение документа. Для contract строка `Не владеет` фиксирует границу ответственности и предотвращает второго владельца.
