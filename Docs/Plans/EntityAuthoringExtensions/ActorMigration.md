---
title: Actor Entity Migration Tasks
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - AuthoringPrototypes.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0030-textsystem-layer-and-data-driven-package-set.md
  - ../../ADR/0031-entity-authoring-extensions.md
---

# M3 — Actor Migration

> **Материализует:** [ADR-0031 § 2.1, 2.3](../../ADR/0031-entity-authoring-extensions.md) в части миграции методов `Actor` в пакетах `textsystem` и `rh`.
> **Задачи:** EAE-08…10.
> **Результат:** `textsystem` и `rh` объявляют методы `Actor` через `function Actor:method()`; ручные декораторы и `register_type` удалены.

## Результат этапа

Методы сущности `Actor` переведены на новый декларативный синтаксис. Пакет `textsystem` объявляет базовые методы (`is_player`, `is_npc`, `require_location`, `move_to`), а пакет `rh` — экономические методы (`get_gold`, `add_gold`, `require_gold`, `spend_gold`, `add_item`). Из обоих пакетов удалены ручные функции-декораторы и низкоуровневые вызовы `register_type`.

## Задачи

- [ ] **EAE-08 — Миграция методов Actor в `textsystem`**
  - Зависимости: EAE-06.
  - Done: методы актора в `GameData/textsystem/` переведены на синтаксис `function Actor:is_player()`, `function Actor:is_npc()`, `function Actor:require_location(loc, opt_key)`, `function Actor:move_to(target)`; старый `actor_decorator` удален; регистрация ссылочного поля `current_location` сохранена.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **EAE-09 — Миграция ресурсных и предметных методов Actor в `rh`**
  - Зависимости: EAE-08.
  - Done: методы ресурсов и предметов в `GameData/rh/` переведены на синтаксис `function Actor:get_gold()`, `function Actor:add_gold(amt)`, `function Actor:require_gold(amt, opt_key)`, `function Actor:spend_gold(amt)`, `function Actor:get_stamina()`, `function Actor:add_stamina(amt)`, `function Actor:require_stamina(amt, opt_key)`, `function Actor:spend_stamina(amt)`, `function Actor:add_item(item)`; ручной `actor_decorator` и `setmetatable` удалены.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **EAE-10 — Очистка `actor_registry.lua` и унификация доступа к экземплярам**
  - Зависимости: EAE-09.
  - Done: `ActorWrapper` получает доступ к методам напрямую из `effective method table` сущности `Actor`, построенной `entity_extension_registry`; ручная цепочка `type_decorators` удалена или упрощена; поведение существующих вызовов методов актора полностью сохранено.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] В `GameData/textsystem` и `GameData/rh` не осталось ручных `register_type` и декораторов акторов.
- [ ] Экземпляры акторов (`player`, `npc`) поддерживают как базовые методы `textsystem`, так и экономические методы `rh`.
- [ ] Спеки `world/` (TextSystem tier) и `economy/` (FullGame tier) проходят без изменений в assertion-кодах.
