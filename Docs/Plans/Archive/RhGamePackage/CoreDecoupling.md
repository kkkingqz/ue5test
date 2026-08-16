---
title: Core Decoupling Tasks
status: archived
version: 1.0
updated: 2026-08-16
depends_on:
  - EntityMigration.md
  - ../../../Architecture/Modding.md
---

# M3 — Core Decoupling

> **Материализует:** [Modding § Identity and content](../../../Architecture/Modding.md).
> **Задачи:** RH-10…13.
> **Результат:** `core` не содержит ни одной ссылки на `rh`, и это удерживается проверкой, а не вниманием.

## Результат этапа

Разделение перестаёт быть состоянием файлов и становится правилом: движок не знает идентификаторов игры, и попытка сослаться на них ломает CI.

## Задачи

- [x] **RH-10 — Развязать демо-экран от конкретного предмета**
  - Зависимости: RH-05.
  - `Scripts/debug/start.lua` берёт `core:item.weapon.iron_sword` жёстко. Экран остаётся в `core` по правилу «экран — возможность движка», поэтому меняется не место модуля, а способ получения предмета.
  - Done: модуль берёт первый элемент `game.repository.list("item")` в каноническом порядке; пустой список не роняет экран, а даёт предсказуемое пустое состояние; ни одного литерала конкретной сущности в модуле не осталось; поведение демо покрыто спекой, не зависящей от того, какие предметы есть в `rh`.
  - Evidence: `Scripts/debug/start.lua`, `Tests/Lua/lifecycle/debug_screen.lua`.

- [x] **RH-11 — Ввести гейт на обратную ссылку**
  - Зависимости: RH-10.
  - Done: проверка ломает CI, если `rh:` встречается в `Scripts/`, `GameData/core/` или `Source/`; проверка зарегистрирована в CTest рядом с `pcc_shared_fixture_contract` и `host_conformance_parity_contract`; сообщение называет файл, строку и правило; негативный тест подтверждает срабатывание.
  - Evidence: `Tools/Content/validate_core_decoupling.py`, `CMakeLists.txt` (`core_decoupling_gate_contract`, `core_decoupling_gate_negative_contract`).

- [x] **RH-12 — Определить поведение старых сейвов**
  - Зависимости: RH-07.
  - Сейв, снятый до переноса, содержит `core:location.*` и `core:actor.*`, которых больше нет.
  - Done: загрузка такого сейва даёт типизированную ошибку отсутствующего Stable ID (`unknown`, не `retired`), а не тихий сбой и не частично восстановленное состояние; поведение покрыто спекой; в [Canonical State and Save](../../../Architecture/CanonicalStateAndSave.md) зафиксировано, что смена namespace сущности — несовместимое изменение, а redirect для неё сознательно не создаётся.
  - Evidence: `Tests/Lua/save/load_path.lua` (`old_save_with_unmigrated_core_location_fails_as_unknown`), `Docs/Architecture/CanonicalStateAndSave.md`.

- [x] **RH-13 — Синхронизировать документацию**
  - Зависимости: RH-10–RH-12.
  - Done: [Modding](../../../Architecture/Modding.md) описывает `rh` как игровой пакет поставки и правило «сущности в `rh`, возможности в `core`»; [Build and Tooling](../../../Architecture/BuildAndTooling.md) описывает набор из двух пакетов, staging и новый гейт; [Concepts/ContentModel](../../../Concepts/ContentModel.md) объясняет разделение читателю; примеры в документации, использующие `core:item.weapon.iron_sword`, обновлены либо заменены синтетическими; [Implementation Status](../../../Status/ImplementationStatus.md) обновлён.
  - Evidence: `Docs/Architecture/{Modding,BuildAndTooling,CanonicalStateAndSave,DefinitionEnvelopeAndSchemaRules,GameDataRepositoryContract,GlossaryAndNaming,StableIDSpecification}.md`, `Docs/Concepts/{ContentModel,GameplayModel}.md`, `Docs/Status/ImplementationStatus.md`.

## Проверка milestone

- [x] Поиск `rh:` по `Scripts/`, `GameData/core/` и `Source/` не даёт совпадений.
- [x] Гейт срабатывает на искусственно добавленной ссылке.
- [x] Демо-экран работает и не знает ни одной конкретной сущности.
- [x] Старый сейв отвергается типизированной ошибкой.
- [x] Документация описывает правило размещения: сущность — в `rh`, возможность — в `core`.
