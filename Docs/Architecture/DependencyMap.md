---
title: Dependency Map
status: normative
version: 1.3
updated: 2026-09-07
depends_on:
  - Overview.md
  - SystemContextAndComponents.md
decisions:
  - ../ADR/0043-presentation-apply-boundary.md
---

# Карта зависимостей

> **Владеет:** ничем — документ навигационный и собственных правил не вводит.
> **Не владеет:** формулировками запретов; они принадлежат contracts по ссылкам.
> **Инварианты:** [INV-007](Invariants.md), [INV-008](Invariants.md), [INV-013](Invariants.md)
> **Реализация:** направления зависимостей закреплены в `Source/CMakeLists.txt` и `*.Build.cs`.
> **Проверки:** `host_conformance_parity_contract`, `core_decoupling_gate_contract`; нарушение направления не собирается.

## Разрешённые направления

```text
External Content            GameData/, Scripts/, Resources/
        ↓
Content Core                GV2ContentCore + GV2ContentHostSupport (+ FResolvedPackageSet, ADR-0043 D1/D5)
        ↓
Lua Gameplay Runtime        GV2RuntimeCore + Scripts/
        ↓
C++ Host Boundary           GV2 (Application, Bridge)
        ↓
UE Presentation (semantic)  GV2 (UI): session content snapshot, Prepare, transaction
        ↓
UE Presentation (apply)     GV2PresentationApply: Commit/rollback/reconciliation (ADR-0043 D2)
        ↓
Unreal Engine only          Core, CoreUObject, Engine, UMG, CommonUI, Slate, SlateCore

core  ←  feature packages  ←  gameplay package  ←  mods
        обратное направление запрещено (ADR-0026)

gv2-headless  →  тот же Content Core + тот же Lua Runtime, без Presentation
gv2-content   →  только Content Core, без Lua VM
```

Стрелка означает «может зависеть». Обратное направление запрещено во всех случаях. `GV2PresentationApply` — единственный узел этой цепочки, чей allowlist обрывается на голом Unreal Engine: ему запрещена зависимость на `GV2ContentCore`, `GV2ContentHostSupport`, `GV2RuntimeCore` и сам `GV2`, поэтому он физически не может прочитать ни один content/authority авторитет, даже если бы захотел (ADR-0043 D2/D4) — граф сборки, а не текстовый скан, первичная гарантия `INV-P5`.

## Запрещённые зависимости

Формулировки ниже сокращены для навигации. Нормативный текст — в указанном contract.

| Запрет | Нормативный источник |
|---|---|
| Content Core не зависит от UE и Presentation | [System Context](SystemContextAndComponents.md), [ADR-0018](../ADR/0018-portable-content-core-module.md) |
| Content Core не выполняет filesystem I/O; discovery принадлежит host-support | [ADR-0018](../ADR/0018-portable-content-core-module.md), [ADR-0019](../ADR/0019-content-host-support-module.md) |
| Lua не получает UObject, указатели, пути к ассетам и filesystem API | [Lua Runtime Contract](LuaRuntimeContract.md), [Overview § Trust model](Overview.md) |
| Canonical gameplay-state не пересекает C++/Lua boundary | [ADR-0021](../ADR/0021-opaque-save-container.md), [Lua Runtime Contract](LuaRuntimeContract.md) |
| Authoring declarations адаптируются в единственный runtime; второго пути мутации и presentation model нет | [Authoring Surface Contract](AuthoringSurfaceContract.md) |
| Top-level `game` и production registries не расширяются ad hoc | [Runtime Facade and Registries](RuntimeFacadeAndRegistries.md) |
| UE Presentation не меняет canonical state и не принимает gameplay-решений | [Overview](Overview.md), [UI Index](../UI/README.md) |
| Headless не требует presentation-состояния и не грузит media | [Headless Simulation Contract](HeadlessSimulationContract.md) |
| Runtime instance ссылается на definition по Stable ID, а не хранит копию | [Canonical State and Save](CanonicalStateAndSave.md), [Stable ID Specification](StableIDSpecification.md) |
| Tooling не становится runtime dependency | [Build and Tooling](BuildAndTooling.md) |
| `gv2-content` не линкует Lua VM | [Build and Tooling](BuildAndTooling.md) |
| `GV2PresentationApply` не зависит от `GV2`, `GV2ContentCore`, `GV2ContentHostSupport`, `GV2RuntimeCore`, `DeveloperSettings`, `AssetRegistry`, `ImageCore` и content authoring modules; запрет — граф сборки, не текстовый скан | [System Context](SystemContextAndComponents.md), [ADR-0043](../ADR/0043-presentation-apply-boundary.md) |
| Подготовленная презентационная транзакция несёт разрешённое значение, а не ссылку/мягкий указатель на резолвер | [UI Document and Reconciliation](../UI/UIDocumentAndReconciliation.md), [ADR-0043](../ADR/0043-presentation-apply-boundary.md) |
| Gameplay и presentation не импортируют `boundary` | [Lua Runtime Contract](LuaRuntimeContract.md) |
| `core` не зависит от игрового пакета: не импортирует его модули, не ссылается на его ID и не предполагает наличия инвентаря, торговли, боя или квестов | [Modding](Modding.md), [ADR-0026](../ADR/0026-core-and-gameplay-ownership.md) |
| Код принадлежит C++ только при выполнении одного из двух условий | [ADR-0020](../ADR/0020-cpp-scope-criterion.md) |

## Физические модули

| Модуль | Роль | Зависит от |
|---|---|---|
| `GV2ContentCore` | Value model, Stable ID, JSON5, схемы, сборка репозитория | — |
| `GV2ContentHostSupport` | Filesystem discovery пакетов | `GV2ContentCore` |
| `GV2RuntimeCore` | Lua VM, сессия, marshalling | `GV2ContentCore` |
| `GV2TestSupport` | Spec runner и тестовые фикстуры | `GV2RuntimeCore` |
| `GV2ContentAuthoring` | Portable atomic authoring operations | `GV2ContentCore`, `GV2ContentHostSupport` |
| `GV2ContentEditor` | Editor Adapter и Unreal Editor Slate frontend | `GV2ContentAuthoring`, `GV2ContentCore`, `GV2ContentHostSupport`; editor-only |
| `GV2` | UE composition, Bridge, Presentation (semantic: snapshot, Prepare, транзакция) | runtime modules, `GV2PresentationApply`; `GV2TestSupport`, `GV2ContentAuthoring`, `GV2ContentEditor` только в Editor target |
| `GV2PresentationApply` (ADR-0043 D2, target module — PSC-11) | Физическое применение: Commit, откат, keyed-реконсиляция, восстановление проекции, чистые расчёты раскладки | `Core`, `CoreUObject`, `Engine`, `UMG`, `CommonUI`, `Slate`, `SlateCore` — только |

Точная физическая раскладка и build-таргеты — [Build and Tooling](BuildAndTooling.md).
