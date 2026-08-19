---
title: External Project Adoption Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-08-13
depends_on:
  - ../Architecture/Overview.md
  - ../Architecture/SystemContextAndComponents.md
  - ../Architecture/LuaRuntimeContract.md
  - ../Architecture/Modding.md
  - ../UI/ScreenTemplates.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
---

# Предложение по использованию внешних архитектурных референсов и модулей

> **Предлагает:** матрицу использования внешних проектов: что подключаем, что берём как референс, что не подключаем.
> **Затрагивает:** [Overview](../Architecture/Overview.md), [System Context](../Architecture/SystemContextAndComponents.md).
> **Не является нормативным:** до реализации действует текущий contract.

## Назначение и область

Документ фиксирует, какие внешние проекты GV2 использует непосредственно, какие — только как источник архитектурных и тестовых решений, а какие не подключает. Цель — получить практическую пользу без второго runtime, тяжёлой framework-зависимости или размытия ownership.

## Принципы выбора

- Готовая dependency принимается только если она закрывает инфраструктурную задачу лучше малого собственного слоя и не нарушает portable/headless boundary.
- Reference-only означает заимствование подходов, test scenarios и UX-паттернов, но не перенос исходного кода.
- Любое копирование кода требует отдельной проверки license, notices, версии и maintenance owner.
- Unreal-specific dependency не может попасть в `GV2RuntimeCore` или будущий `GV2ContentCore`.
- Новый framework не принимается ради будущей возможности без текущего consumer и измеренной пользы.

## Матрица решений

| Проект | Решение для GV2 | Польза | Основной риск | Трудоёмкость |
|---|---|---|---|---|
| [OpenVic](https://github.com/OpenVicProject/OpenVic-Simulation) | Reference-only | Разделение loading/definitions/runtime, coordinator, headless tests | Перенос ECS, fixed-point или per-type managers усложнит GV2 | S для анализа, L для собственного Content pipeline |
| [Project Alice](https://github.com/schombert/Project-Alice) | Reference-only scenarios | Большой набор случаев definitions, references, mod overlay, save/load | Paradox-specific model и GPL-код нельзя делать архитектурной основой | M |
| [Metternich](https://github.com/Andrettin/Metternich) | Conceptual reference | Граница engine/content | Незрелые и domain-specific решения | S |
| [rakaly/jomini](https://github.com/rakaly/jomini) | Reference-only methods | Source location, bounded parsing, fuzzing, benchmark discipline | Rust/Paradox parser не нужен JSON5 pipeline | M |
| [CWTools](https://github.com/cwtools/cwtools) и [Paradox Language Support](https://github.com/DragonKnightOfBreeze/Paradox-Language-Support) | Future tooling reference | Validation, completion, references и editor diagnostics | Отдельная grammar/validator implementation создаст drift | L, только после shared CLI |
| [UnLua](https://github.com/Tencent/UnLua) | Не подключать | Богатая UE reflection integration не нужна узкому Game API | UObject/reflection/callback model и UE lifetime нарушают value-only portable boundary | S: зафиксировать отказ |
| [LuaMachine](https://github.com/rdeioris/LuaMachine) и [sluaunreal](https://github.com/Tencent/sluaunreal) | Reference-only VM lifecycle | Error handling, VM lifecycle и hot reload cases | Второй bridge/runtime и прямой UE access | S |
| [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) | Mod UX reference-only | Discovery, folder conventions, enable order и diagnostics UX | Direct UObject/native access не соответствует trust boundary | M |
| [Unreal Data Registry](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-registries-in-unreal-engine) | Pattern reference-only | Source lifecycle, read-only access, cache/acquire semantics | Не покрывает GV2 namespaces, deterministic overrides и provenance | S |
| [DataConfig](https://github.com/slowburn-dev/DataConfig) | Отложить | UE reflection serialization может пригодиться UE-only DTO | Лишняя dependency до появления конкретного codec consumer | S сейчас, M при внедрении |
| [CommonUI](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine) | Использовать непосредственно | Input routing, focus, activatable lifecycle и reusable navigation primitives | Возможна конкуренция с Lua desired UI-document | M |
| [UMG Designer](https://dev.epicgames.com/documentation/en-us/unreal-engine/widget-blueprints-in-umg-for-unreal-engine) | Использовать непосредственно | Зрелый visual authoring Screen Templates | Полный Widget Tree round-trip создаст второй source of truth | M для минимального tooling |

## Принятые архитектурные выводы

1. GV2 сохраняет собственный узкий Lua host на pinned Lua 5.4.8. UnLua, LuaMachine и sluaunreal не становятся runtime dependencies.
2. Data layer строится как portable pipeline с immutable definitions и repository snapshot; runtime state остаётся отдельным Lua-owned слоем.
3. OpenVic, Project Alice и Metternich используются для проверки границ и сценариев, но не для переноса framework-структуры.
4. Jomini задаёт качество diagnostics/testing, а не parser technology.
5. CommonUI и UMG используются внутри UE Presentation; ни один из них не получает gameplay authority.
6. UE4SS влияет только на UX управления модами. Native libraries, UObject reflection и произвольный UE API для mod Lua запрещены.
7. Unreal Data Registry и DataConfig не заменяют GameDataRepository.

## Связанные предложения

- [Portable Content Core](Archive/PortableContentCoreProposal.md)
- [Content Diagnostics and Tooling](ContentDiagnosticsAndToolingProposal.md)
- [Mod Package Lifecycle](ModPackageLifecycleProposal.md)
- [CommonUI Runtime Integration](CommonUIRuntimeIntegrationProposal.md)
- [Screen Authoring Workflow](ScreenAuthoringWorkflowProposal.md)

## Риски и меры

- **License contamination.** До переноса любого фрагмента кода обязателен third-party review; по умолчанию переносятся только идеи и independently written tests.
- **Architecture by imitation.** Каждое решение проверяется против GV2 contracts и concrete vertical slice.
- **Dependency sprawl.** Любая новая dependency имеет owner, pinned version, manifest/checksum, license notice и removal plan.
- **Duplicate authority.** Внешний framework используется только внутри уже определённого owner layer.

## Критерии приёмки

- Build manifests не содержат UnLua, LuaMachine, sluaunreal, DataConfig или runtime dependency на перечисленные reference-only проекты.
- CommonUI остаётся UE-only dependency и не появляется в portable targets.
- Реализация каждого связанного proposal обновляет owner contracts и при необходимости добавляет ADR до изменения архитектурного инварианта.
- Third-party inventory различает direct dependencies, tools и reference-only sources.
