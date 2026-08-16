---
title: Concepts Index
status: informative
version: 1.0
updated: 2026-08-15
depends_on:
  - ../README.md
---

# Концепции GV2

Этот раздел объясняет понятия GV2 обычным техническим языком: что это, зачем и как связано с остальным. Он предназначен для первого знакомства, для быстрого ответа на локальный вопрос и как первый шаг перед чтением нормативного contract.

**Раздел не является нормативным.** Он не вводит архитектурных правил. Если объяснение расходится с contract или ADR, прав contract. Каждое утверждение об обязательном поведении сопровождается ссылкой на нормативный источник.

## Документы

| Документ | О чём |
|---|---|
| [GameplayModel](GameplayModel.md) | Как игрок влияет на состояние: команды, валидаторы, сервисы, события, presentation |
| [ContentModel](ContentModel.md) | Откуда берутся definitions: пакеты, схемы, репозиторий, ссылки |
| [RuntimeInstances](RuntimeInstances.md) | Разница между definition и экземпляром: акторы, обёртки, реестры, identity |
| [PresentationModel](PresentationModel.md) | Как желаемое состояние экрана превращается в UMG |
| [DeterminismAndTesting](DeterminismAndTesting.md) | Почему всё воспроизводимо и как это проверяется |

## Концепция → contract → код → тесты

Навигационная карта для ключевых понятий. Нормативным остаётся contract; таблица только указывает, где искать.

| Понятие | Contract | Реализация | Проверки |
|---|---|---|---|
| Definition, schema, repository | [GameDataRepository](../Architecture/GameDataRepositoryContract.md), [DefinitionEnvelope](../Architecture/DefinitionEnvelopeAndSchemaRules.md) | `Source/GV2ContentCore/` | `RunJson5*Conformance`, `RunSchemaRegistryConformance` |
| Stable ID | [StableIDSpecification](../Architecture/StableIDSpecification.md) | `Source/GV2ContentCore/Private/StableId.cpp`, `Scripts/runtime/stable_id.lua` | `RunStableIdConformance` |
| Canonical state | [CanonicalStateAndSave](../Architecture/CanonicalStateAndSave.md) | `Scripts/runtime/state_validator.lua` | `Tests/Lua/lifecycle/state_sections.lua` |
| Mutation window | [CommandsAndEvents](../Architecture/CommandsAndEvents.md) | `Scripts/runtime/mutation_window.lua` | `Tests/Lua/lifecycle/mutation_window.lua` |
| Actor registry, обёртки | [LuaRuntimeContract](../Architecture/LuaRuntimeContract.md) | `Scripts/runtime/actor_registry.lua` | `Tests/Lua/world/domain_object.lua` |
| Команды и валидаторы | [CommandsAndEvents](../Architecture/CommandsAndEvents.md) | `Scripts/runtime/command_dispatcher.lua`, `validator_registry.lua` | `Tests/Lua/commands/` |
| События и подписки | [CommandsAndEvents](../Architecture/CommandsAndEvents.md) | `Scripts/runtime/event_bus.lua`, `subscriber_registry.lua` | `Tests/Lua/events/` |
| Канонический хэш и digest | [HeadlessSimulation](../Architecture/HeadlessSimulationContract.md) | `Scripts/runtime/canonical_codec.lua`, `state_hasher.lua` | `Tests/Lua/save/canonical_codec.lua` |
| Экраны и Screen Fields | [ScreenTemplates](../UI/ScreenTemplates.md) | `Source/GV2/Private/UI/`, `Scripts/presentation/` | `GV2.Runtime.Presentation.*` |
| Текст и локализация | [BuildAndTooling](../Architecture/BuildAndTooling.md) | `Source/GV2ContentCore/Private/PoParser.cpp` | `RunPoParserConformance` |

Что из этого уже реализовано и в каком объёме — в [Implementation Status](../Status/ImplementationStatus.md).
