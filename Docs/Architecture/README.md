---
title: Architecture Index
status: normative
version: 1.1
updated: 2026-08-16
depends_on:
  - ../README.md
---

# Архитектурные контракты

Раздел нормативный: он определяет, как GV2 обязан работать. Объяснения «зачем и как понимать» живут в [Concepts](../Concepts/README.md), инструкции «как сделать» — в [Guides](../Guides/README.md), причины выбора — в [ADR](../ADR/README.md).

## Навигация

| Документ | Когда читать |
|---|---|
| [Overview](Overview.md) | Источники истины, слои, границы C++, non-goals |
| [Dependency Map](DependencyMap.md) | Кто от кого может зависеть и что запрещено |
| [Invariants](Invariants.md) | Стабильные ID инвариантов и где их нормативный текст |
| [Glossary and Naming](GlossaryAndNaming.md) | Термины, имена, категории ID |
| [System Context and Components](SystemContextAndComponents.md) | Модули, ownership, lifetime |
| [Bootstrap and Session Lifecycle](BootstrapAndSessionLifecycle.md) | Cold start, session, restart, teardown |
| [Lua Runtime Contract](LuaRuntimeContract.md) | VM, модули, фасад `game`, значения |
| [Commands and Events](CommandsAndEvents.md) | Команды, валидаторы, mutation window, события, фазы |
| [Canonical State and Save](CanonicalStateAndSave.md) | Форма состояния, инварианты экземпляров, сейв |
| [GameDataRepository Contract](GameDataRepositoryContract.md) | Сборка репозитория, override, снимок, чтение |
| [Definition Envelope and Schema Rules](DefinitionEnvelopeAndSchemaRules.md) | Конверт файла, схемы, значения по умолчанию |
| [Stable ID Specification](StableIDSpecification.md) | Грамматика, namespace, redirects, жизненный цикл |
| [Headless Simulation Contract](HeadlessSimulationContract.md) | Роли UE-free хоста, conformance, manifest и digest |
| [Build and Tooling](BuildAndTooling.md) | Таргеты, хосты, CLI, фикстуры, CI |
| [Modding](Modding.md) | Пакеты модов, границы доверия |

## Header документов

Каждый документ открывается блоком-цитатой сразу после заголовка. Первое поле обязательно и проверяется валидатором; набор полей зависит от типа документа, потому что полезный вопрос у типов разный.

| Раздел | Обязательное поле | Остальные поля |
|---|---|---|
| `Architecture/`, `UI/` | **Владеет** | Не владеет, Инварианты, Реализация, Проверки |
| `Concepts/` | **Объясняет** | Нормативно, Не является нормативным |
| `Guides/` | **Задача** | Нужно, Нормативно |
| `Authoring/` | **Задача** | Нужно, Проверка |
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

Header **не повторяет front matter**. Статус, зависимости и связанные ADR уже объявлены машинно-читаемо и проверяются валидатором; дублировать их прозой значит завести второй источник, который разойдётся молча. Назначение документа тоже не дублируется: его несёт первый абзац.

Строка `Не владеет` для GV2 особенно полезна. Архитектура построена на единственном владельце данных, и типичная ошибка проектирования — не спор о правилах, а тихое присвоение соседней ответственности.

Header не имеют индексы (`README.md`) — они сами являются навигацией, — и архивные планы: это исторические записи, которые не переписываются.
