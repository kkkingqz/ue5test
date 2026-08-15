---
title: State and Isolation Tasks
status: draft
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ../../Architecture/CanonicalStateAndSave.md
decisions:
  - ../../ADR/0024-lua-spec-runner.md
---

# M2 — State and Isolation

## Результат этапа

Структура канонического состояния `game.state`, изоляция секций, запрет прямых циклических ссылок и правила генерации `instance_id` покрыты декларативными спеками в `Tests/Lua/lifecycle/`.

## Задачи

- [x] **LSM-03 — Спека на структуру и валидатор секций `game.state`**
  - Описание: Спека проверяет наличие 6 обязательных секций (`meta`, `actors`, `item_instances`, `world`, `quests`, `mods`), отказ на неизвестных секциях верхнего уровня, отказ на недопустимых типах значений и ссылочную целостность акторов/предметов.
  - Done: Спека `Tests/Lua/lifecycle/state_sections.lua` проверяет инварианты структуры состояния.
  - Evidence: Создан `Tests/Lua/lifecycle/state_sections.lua` (кейсы `canonical_state_structure_has_required_sections`, `state_validator_allows_valid_state`, `state_validator_rejects_unknown_root_section`, `state_validator_rejects_function_value`, `state_validator_rejects_table_with_metatable`, `state_validator_rejects_nan_and_infinity`, `state_validator_rejects_cyclic_reference`, `state_validator_rejects_definition_table_embedding`); `Scripts/runtime/state_validator.lua` усилен проверкой неизвестных верхнеуровневых секций; `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **LSM-04 — Спека на аллокатор `instance_id` и изоляцию прототипов**
  - Описание: Спека проверяет монотонность аллокатора `core:module.runtime.instance_allocator`, формат идентификаторов экземпляров (`<kind>@<number>`), невозможность коллизий и изоляцию метатаблиц акторов/предметов от утечек глобального состояния.
  - Done: Спека `Tests/Lua/lifecycle/instance_identity.lua` проверяет генерацию идентификаторов и инварианты изоляции.
  - Evidence: Создан `Tests/Lua/lifecycle/instance_identity.lua` (кейсы `instance_allocator_allocates_sequential_ids`, `instance_allocator_respects_existing_counter`, `instance_allocator_rejects_invalid_kind`, `instance_allocator_parses_and_formats`, `instance_objects_are_isolated`); `gv2-headless --self-test` и CTest (57/57 passed).

## Проверка milestone

- [x] Инварианты всех 6 секций состояния проверяются спеками.
- [x] Аллокация и формат `instance_id` проверяются спеками.
