---
title: GV2 Documentation Index
status: normative
version: 1.0
updated: 2026-08-10
language: ru
---

# Документация GV2

Этот каталог — единая нормативная документация проекта. Markdown-файлы предназначены одновременно для разработчиков, инструментов и AI-агентов.

## Статус источников

1. `Docs/ADR/*.md` с `status: accepted` фиксируют принятые архитектурные решения.
2. Контракт конкретной подсистемы уточняет `Architecture/Overview.md`.
3. `Architecture/Overview.md` задаёт общие границы и инварианты.
4. Экспортированные, архивные и внешние копии не являются нормативными и не должны храниться в `Docs` рядом с canonical Markdown.

При конфликте применяется более конкретный документ. Если accepted ADR был заменён, он обязан содержать `status: superseded` и ссылку на замену.

## Быстрый маршрут для AI

Для общей задачи читать в таком порядке:

1. [Architecture/Overview.md](Architecture/Overview.md)
2. [Architecture/GlossaryAndNaming.md](Architecture/GlossaryAndNaming.md)
3. Контракт затронутой подсистемы из таблицы ниже.
4. Связанные accepted ADR.

Не загружать все документы без необходимости. Каждый контракт содержит собственные зависимости и инварианты.

## Зафиксированные решения v1

- Stable ID: `<namespace>:<kind>.<path>`, только canonical lowercase ASCII.
- Lua — единственный владелец canonical gameplay-state.
- Gameplay меняется через `Command Dispatcher`; EventBus публикует только post-commit факты.
- C++/Lua boundary передаёт только DTO, Stable ID и opaque operation handles; C++ не хранит Lua callbacks.
- Одна Lua VM на session, owner-thread only, без synchronous re-entry; UE использует Game Thread.
- GameDataRepository публикует только целый immutable snapshot; session закрепляет snapshot до restart.
- Content reload применяется через controlled session restart.
- UI — полная декларативная desired model; presentation effects не участвуют в восстановлении.
- Full override by ID; deep merge и универсальный patch language отсутствуют.
- Опубликованный Stable ID не переиспользуется для другого смысла.

## Карта документов

### Architecture

| Документ | Когда читать |
|---|---|
| [Overview](Architecture/Overview.md) | Источники истины, слои, основные потоки и non-goals |
| [Glossary and Naming](Architecture/GlossaryAndNaming.md) | Термины, имена, категории ID |
| [System Context and Components](Architecture/SystemContextAndComponents.md) | Модули, зависимости, ownership и lifetime |
| [Bootstrap and Session Lifecycle](Architecture/BootstrapAndSessionLifecycle.md) | Cold start, menu/game session, load, restart, teardown |
| [Lua Runtime Contract](Architecture/LuaRuntimeContract.md) | VM, modules, values, state, determinism и ingress |
| [Headless Simulation Contract](Architecture/HeadlessSimulationContract.md) | Portable runtime, simulation driver, deterministic batches и metrics |
| [Stable ID Specification](Architecture/StableIDSpecification.md) | Grammar, namespace ownership, redirects и instance IDs |
| [Definition Envelope and Schema Rules](Architecture/DefinitionEnvelopeAndSchemaRules.md) | JSON5 envelope, schemas, defaults, extensions и validation |
| [GameDataRepository Contract](Architecture/GameDataRepositoryContract.md) | Build pipeline, overrides, snapshot API и reload |
| [Commands and Events](Architecture/CommandsAndEvents.md) | Command lifecycle, validation, mutation и post-commit events |
| [Canonical State and Save](Architecture/CanonicalStateAndSave.md) | State shape, save boundary, load и migrations |
| [Modding](Architecture/Modding.md) | Package ownership, Lua modules, overrides и trust model |

### UI

| Документ | Когда читать |
|---|---|
| [UI Index](UI/README.md) | Маршрут по UI-контрактам |
| [UI Document and Reconciliation](UI/UIDocumentAndReconciliation.md) | Route, layers, nodes, keys и full reconciliation |
| [Semantic Input](UI/SemanticInput.md) | UE → Lua input envelope и stale-input rejection |
| [Presentation Snapshot and Effects](UI/PresentationSnapshotAndEffects.md) | Durable desired presentation и one-shot effects |
| [Widget Registry](UI/WidgetRegistry.md) | `widget_id`, factories, mod widgets и fallback |

### ADR

Индекс решений: [ADR/README.md](ADR/README.md).

## Правила обновления

- Сначала обновить или добавить ADR, если меняется источник истины, ID, command/event semantics, save compatibility, trust model или module dependency direction.
- Затем обновить все затронутые контракты и примеры в том же изменении.
- Публичный пример считается тестовым fixture: он обязан соответствовать Stable ID grammar и терминологии.
- Новая абстракция добавляется только для конкретного сценария vertical slice или измеренной проблемы.
