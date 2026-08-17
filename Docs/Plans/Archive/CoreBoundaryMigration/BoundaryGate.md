---
title: Boundary Gate Tasks
status: archived
version: 1.0
updated: 2026-08-17
depends_on:
  - SchemaMigration.md
  - ../../../Architecture/BuildAndTooling.md
---

# M4 — Boundary Gate

> **Материализует:** [ADR-0026](../../../ADR/0026-core-and-gameplay-ownership.md) как проверку, а не соглашение.
> **Задачи:** CBM-14…16.
> **Результат:** новая игровая сущность в ядре ломает CI.

## Результат этапа

Граница перестаёт держаться на внимании ревьюера. Правило «новая сущность сразу в правильном пакете» проверяется машинно, а список известных отклонений пуст.

## Задачи

- [x] **CBM-14 — Гейт на игровые сущности в ядре**
  - Зависимости: CBM-11.
  - Done: проверка ломает CI, если в `GameData/core/definitions/` появляется definition kind `actor`, `item` или `location`, либо если `GameData/core/schemas/` содержит схему для этих kind; сообщение называет файл, ID и правило; негативный тест подтверждает срабатывание; проверка зарегистрирована в CTest рядом с `core_decoupling_gate_contract`.
  - Evidence: Создан `Tools/Content/validate_core_boundary.py` (правило `CORE_GAMEPLAY_BOUNDARY_RULE`). Тесты `core_boundary_gate_contract` и `core_boundary_gate_negative_contract` зарегистрированы в `CMakeLists.txt`.

- [x] **CBM-15 — Закрыть список известных нерегистраций**
  - Зависимости: CBM-12.
  - Done: список известных незарегистрированных discriminator пуст; временный режим базовой обёртки снят, незарегистрированный discriminator даёт типизированный отказ безусловно; negative case подтверждает отказ.
  - Evidence: Из `Scripts/runtime/actor_registry.lua` удалён `KNOWN_UNREGISTERED`, незарегистрированный `discriminator` безусловно вызывает `ActorTypeNotRegistered`. Спека `Tests/Lua/actors/actor_extension.lua` подтверждает отказ.

- [x] **CBM-16 — Синхронизировать документацию**
  - Зависимости: CBM-14, CBM-15.
  - Done: [Build and Tooling](../../../Architecture/BuildAndTooling.md) описывает новый гейт; [Concepts/ContentModel](../../../Concepts/ContentModel.md) объясняет разделение владения читателю; `Docs/Authoring/` описывает, что схемы игры живут в её пакете; [Implementation Status](../../../Status/ImplementationStatus.md) обновлён; в proposal проставлено `proposal_state: implemented` с указанием, какие разделы перенесены в contracts.
  - Evidence: Обновлены `BuildAndTooling.md`, `ContentModel.md`, `Docs/Authoring/README.md`, `Docs/Authoring/Objects.md`, `ImplementationStatus.md`, `CoreGameplayBoundaryProposal.md`. Валидация документации `validate_docs.py` — 161/161 файлов валидны.

## Проверка milestone

- [x] Добавление `core:item.*` в ядро ломает CI.
- [x] Список известных нерегистраций пуст, временный режим снят.
- [x] Документация описывает границу одинаково в contracts, Concepts и Authoring.
