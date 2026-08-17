---
title: Instance Extension Tasks
status: draft
version: 1.0
updated: 2026-08-17
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
---

# M2 — Instance Extension

> **Материализует:** [ADR-0026](../../ADR/0026-core-and-gameplay-ownership.md) в части точки расширения обёртки.
> **Задачи:** CBM-04…08.
> **Результат:** поведение инстанса определяет пакет, идентичность — ядро.

## Результат этапа

Ядро строит базовую обёртку и удерживает инварианты идентичности. Пакет регистрирует декоратор по discriminator и добавляет свои методы. Экономика уезжает из ядра.

Этап обязателен до M3: как только поля актора уедут в пакет, ядро перестанет знать их структуру.

## Задачи

- [x] **CBM-04 — Ввести реестр декораторов обёртки**
  - `game.instances.actors.register_type(discriminator, decorator)`, где `decorator(base) -> wrapper`.
  - Done: регистрация только на фазе `register`, freeze вместе с остальными реестрами; дубликат discriminator — типизированная ошибка; декоратор обязан быть функцией; возвращённое значение обязано быть таблицей; `ids()` / `types()` перечисляет зарегистрированные discriminator в детерминированном порядке.
  - Evidence: Реестр реализован в `Scripts/runtime/actor_registry.lua`. `FreezeGameRegistry({"instances", "actors"})` добавлен в `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`. Спеки в `Tests/Lua/actors/actor_extension.lua` подтверждают отказ на дубликат, freeze, валидацию типов и детерминизм.

- [x] **CBM-05 — Собирать обёртку через декоратор**
  - Зависимости: CBM-04.
  - Done: базовая обёртка сохраняет неизменяемость `instance_id`, `definition_id`, `discriminator` и делегирование в состояние инстанса; декоратор применяется поверх через `__index = base`; обёртка остаётся одноразовой и не кэшируется; попытка декоратора нарушить инвариант идентичности отклоняется, а не переопределяет его.
  - Evidence: `wrap_actor` в `Scripts/runtime/actor_registry.lua` оборачивает декорированный объект защитным прокси. Спека `identity_invariants_preserved` в `Tests/Lua/actors/actor_extension.lua` проверяет защиту идентичности и мутацию свойств состояния.

- [x] **CBM-06 — Определить поведение при отсутствии регистрации**
  - Зависимости: CBM-05.
  - Done: незарегистрированный discriminator даёт типизированный отказ; на время миграции действует базовая обёртка плюс список известных нерегистраций, который проверяется и может только сокращаться; устаревшая запись в списке — ошибка проверки; negative case на оба состояния.
  - Evidence: `wrap_actor` выбрасывает `ActorTypeNotRegistered` при неизвестном дискриминаторе. Спека `unregistered_discriminator_handling` проверяет оба случая.

- [x] **CBM-07 — Перенести экономику в `rh`**
  - Зависимости: CBM-05.
  - `get_gold`/`add_gold` из `Scripts/runtime/actor_registry.lua` — в декоратор пакета.
  - Done: методы регистрируются пакетом для своего discriminator; `actor_registry.lua` не содержит имён из словаря игры; сервис `rh:service.economy` продолжает работать и использует обёртку, а не пишет в состояние напрямую; спеки слайса проходят без изменений в ожиданиях.
  - Evidence: Создан `GameData/rh/scripts/gameplay/actors.lua`, `Scripts/runtime/actor_registry.lua` очищен от `gold`. Спеки `Tests/Lua/economy/economy_service.lua` и `Tests/Lua/actions/location_actions.lua` проходят без ошибок.

- [x] **CBM-08 — Синхронизировать contract**
  - Зависимости: CBM-04–CBM-07.
  - Done: [Lua Runtime Contract](../../Architecture/LuaRuntimeContract.md) описывает `game.instances.actors.register_type`, порядок применения декоратора и инварианты базовой обёртки; [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md) уточняет, что доменные методы не являются частью состояния; guide по добавлению обёртки обновлён.
  - Evidence: Обновлены `Docs/Architecture/LuaRuntimeContract.md` и `Docs/Guides/AddActorWrapper.md`. `validate_docs.py` подтверждает валидность ссылок и фронтматтера.

## Проверка milestone

- [x] Пакет добавляет метод актору, не трогая ядро.
- [x] Инварианты идентичности удерживаются ядром и не переопределяются декоратором.
- [x] `Scripts/runtime/actor_registry.lua` не содержит `gold`.
- [x] Слайс TestGameplaySlice проходится целиком.
