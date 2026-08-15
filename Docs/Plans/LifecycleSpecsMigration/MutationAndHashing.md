---
title: Mutation and Hashing Tasks
status: draft
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ../../Architecture/CommandsAndEvents.md
  - ../../Architecture/CanonicalStateAndSave.md
decisions:
  - ../../ADR/0024-lua-spec-runner.md
---

# M3 — Mutation and Hashing

## Результат этапа

Механизм `mutation_window`, изоляция мутаций состояния и канонический хэшер состояния `core:module.runtime.state_hasher` покрыты декларативными спеками в `Tests/Lua/lifecycle/`.

## Задачи

- [x] **LSM-05 — Спека на окно мутации `mutation_window`**
  - Описание: Спека проверяет запрет прямой записи в `game.state` вне окна мутации, открытие/закрытие окна диспетчером команд, защиту от реентерабельности и изоляцию мутаций при ошибках обработчиков.
  - Done: Спека `Tests/Lua/lifecycle/mutation_window.lua` проверяет ограничения окна мутации.
  - Evidence: Создан `Tests/Lua/lifecycle/mutation_window.lua` (кейсы `direct_mutation_outside_window_rejected`, `mutation_allowed_inside_window`, `nested_table_mutation_protected`, `window_closed_after_exception`); `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **LSM-06 — Спека на канонический хэшер состояния `state_hasher`**
  - Описание: Спека проверяет детерминированное вычисление SHA-256 хэша состояния в чистом Lua, инвариантность к порядку ключей в таблицах, чувствительность к любым изменениям скаляров и структур данных, а также совпадение хэшей для идентичных состояний.
  - Done: Спека `Tests/Lua/lifecycle/state_hasher.lua` проверяет детерминизм и криптографическую корректность хэширования состояния.
  - Evidence: Создан `Tests/Lua/lifecycle/state_hasher.lua` (кейсы `sha256_known_rfc_vectors`, `state_hasher_deterministic_independent_of_key_order`, `state_hasher_changes_when_any_value_changes`, `state_hasher_distinguishes_null_from_absent`); `gv2-headless --self-test` и CTest (57/57 passed).

## Проверка milestone

- [x] Защита от мутаций вне окна проверяется спеками.
- [x] Детерминизм канонического хэша состояния проверяется спеками.
