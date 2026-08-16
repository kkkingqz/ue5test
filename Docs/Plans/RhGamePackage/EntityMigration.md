---
title: Entity Migration Tasks
status: draft
version: 1.0
updated: 2026-08-16
depends_on:
  - PackageSetup.md
  - ../../Architecture/StableIDSpecification.md
---

# M2 — Entity Migration

> **Материализует:** [Stable ID Specification § Namespace ownership](../../Architecture/StableIDSpecification.md).
> **Задачи:** RH-05…09.
> **Результат:** конкретные сущности игры, их тексты, переводы и ресурсы живут в `rh`.

## Результат этапа

11 definitions переехали из `core` в `rh` со сменой namespace. Каждая сущность перенесена целиком: definition, её текст, её ресурс и запись в PO-каталоге.

Перенос выполняется `gv2-content rename`, а не текстовой заменой: инструмент проверяет `package_frozen` и находит обратные ссылки, которые глазами теряются.

## Задачи

- [x] **RH-05 — Перенести предметы**
  - Зависимости: RH-04.
  - `core:item.weapon.iron_sword` → `rh:item.weapon.iron_sword`; вместе с ним `core:text.item.iron_sword.name` и `core:resource.item.iron_sword.icon`.
  - Done: definition, текст и ресурс лежат в `GameData/rh/definitions/`; записи PO перенесены в `GameData/rh/localization/ru.po` и удалены из каталога `core`; файлы изображений переехали в `Resources/rh/` с сохранением grammar имени; `GameData/core/definitions/items.json5` и `resources.json5` удалены как пустые, а не оставлены с пустым массивом.
  - Evidence: `GameData/rh/definitions/{items,texts,resources}.json5`, `GameData/rh/localization/ru.po`, `gv2-content coverage GameData/rh --locale=ru`.

- [x] **RH-06 — Перенести акторов**
  - Зависимости: RH-04.
  - `core:actor.character.hero`, `core:actor.npc.merchant` и их тексты.
  - Done: definitions и тексты в `rh`; `state.meta.player_actor_id` и любые фикстуры, ссылающиеся на актора-игрока, используют новый ID; `discriminator` (`player`/`npc`) остаётся полем схемы `core` и не меняется.
  - Evidence: `GameData/rh/definitions/actors.json5`, `GameData/core/definitions/actors.json5` удалён.

- [x] **RH-07 — Перенести локации**
  - Зависимости: RH-04.
  - `core:location.city.market`, `core:location.city.tavern` и их тексты.
  - Done: definitions и тексты в `rh`; `state.world.current_location_id` принимает `rh:location.*` — реестр ссылочных полей `state_validator` проверяет kind, а не namespace, и правки не требует; команда `core:command.location.travel` работает без изменений.
  - Evidence: `GameData/rh/definitions/locations.json5`, `GameData/core/definitions/locations.json5` удалён, `Tests/Lua/world/` спеки.

- [x] **RH-08 — Обновить Lua-спеки**
  - Зависимости: RH-05–RH-07.
  - Спеки под `Tests/Lua/{world,events,resources,save}` исполняются на боевом репозитории и используют перенесённые ID.
  - Done: спеки ссылаются на `rh:*`; спеки, проверяющие грамматику и отказы, продолжают использовать синтетические ID и не завязываются на игровой контент; ни одна спека не пинит `repository_content_hash` боевого набора.
  - Evidence: `Tests/Lua/{world,events,save}/` спеки обновлены и проходят в CTest.

- [x] **RH-09 — Проверить целостность переноса**
  - Зависимости: RH-05–RH-08.
  - Done: `gv2-content refs` не находит ни одной ссылки на перенесённые `core:*` ID; `gv2-content coverage` показывает полноту перевода `rh` не ниже прежней; репозиторий собирается без диагностик; `foreign_new_id` не возникает — ни один `rh:*` ID не объявлен как override; игра запускается, демо-экран отображается, переход между локациями работает.
  - Evidence: `gv2-content validate GameData/core GameData/rh`, `gv2-content coverage GameData/rh`, 57/57 CTest passed, `gv2-headless --check-scripts`, `gv2-headless --self-test`.

## Проверка milestone

- [x] Ни один `core:item.*`, `core:actor.*`, `core:location.*` не существует в `GameData/`.
- [x] Тексты и ресурсы перенесённых сущностей лежат в `rh` вместе с ними.
- [x] PO-каталог `core` не содержит записей перенесённых текстов.
- [x] Переход между локациями работает на `rh:location.*`.
- [x] Golden-прогон и его digest не изменились.
