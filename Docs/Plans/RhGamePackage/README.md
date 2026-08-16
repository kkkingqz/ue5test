---
title: RH Game Package Implementation Plan
status: normative
version: 1.0
updated: 2026-08-16
depends_on:
  - ../../Architecture/Modding.md
  - ../../Architecture/StableIDSpecification.md
  - ../../Architecture/GameDataRepositoryContract.md
  - ../../Architecture/BuildAndTooling.md
decisions:
  - ../../ADR/0023-stable-id-publication-freeze.md
  - ../../ADR/0025-lua-module-replacement-and-export-freezing.md
---

# План выделения игрового пакета `rh`

> **Материализует:** [Modding](../../Architecture/Modding.md) и [Stable ID Specification](../../Architecture/StableIDSpecification.md) как фактическое разделение поставки.
> **Задачи:** RH-01…13.
> **Результат:** конкретные сущности игры живут в пакете `rh`, `core` не знает ни одного её идентификатора.

## Цель

Разделить разработку на два уровня. `core` остаётся движком: схемы, механики, экраны, команды, события — всё, что должно работать в любой игре на GV2. `rh` становится игрой: конкретные предметы, персонажи, локации и привязанные к ним тексты и ресурсы.

На этом шаге переносятся **только конкретные сущности**. Механика, новый экран или новая возможность по-прежнему добавляются в `core` — по мере наполнения `rh` пополняться будет и он.

Возможность выделить пакет уже есть: план [PackageSupport](../Archive/PackageSupport/README.md) дал обязательный манифест, набор корней с явным порядком, `mods.lock`, Lua внутри пакета и замещение модулей. Настоящий план этой возможностью пользуется — новых механизмов не вводит.

## Состояние на входе

`GameData/core` содержит 14 definitions в шести файлах. По назначению они делятся так:

| Файл | Переносится в `rh` | Остаётся в `core` |
|---|---|---|
| `items.json5` | `core:item.weapon.iron_sword` | — |
| `actors.json5` | `core:actor.character.hero`, `core:actor.npc.merchant` | — |
| `locations.json5` | `core:location.city.market`, `core:location.city.tavern` | — |
| `texts.json5` | `text.item.iron_sword.name`, `text.location.market.title`, `text.location.tavern.title`, `text.character.hero.name`, `text.npc.merchant.name` | `text.screen.main.title`, `text.screen.inventory.title` |
| `resources.json5` | `core:resource.item.iron_sword.icon` | — |
| `screens.json5` | — | `core:screen.main`, `core:screen.inventory` |
| `schemas/*.schema.json5` | — | все шесть |

Инфраструктура готова наполовину: `DiscoverPackagesFromDirectories`, мульти-корневой provider и `mods.lock` реализованы, но **боевые хосты по-прежнему грузят один корень**. `Headless/Source/main.cpp` жёстко указывает `GameData/core`, `--content-root=` принимает одно значение, UE-provider настроен так же.

Ссылки на конкретные сущности из `core` есть в двух местах: `Scripts/debug/start.lua` берёт `core:item.weapon.iron_sword`, а Lua-спеки под `Tests/Lua/{world,events,resources,save}` используют локации и акторов — они исполняются на боевом репозитории, а не на замороженном корпусе.

## Принятые решения

- **ID меняют namespace, redirect не создаётся.** `core:item.weapon.iron_sword` становится `rh:item.weapon.iron_sword`. Ничего не опубликовано, поэтому действует авторский rename до релиза ([ADR-0023](../../ADR/0023-stable-id-publication-freeze.md)). Redirect из `core` в `rh` был бы ссылкой движка на игру и запрещён по смыслу разделения.
- **Схемы остаются в `core`.** Схема объявляет, что такое предмет, актор и локация — это заявление движка о предметной модели. `rh` пользуется схемами `core` и своих не заводит: новый binding нужен только для нового kind.
- **Демо-экран остаётся в `core`, но перестаёт знать конкретный предмет.** По правилу «экран — это возможность движка» `debug/start.lua` не переезжает. Вместо жёсткого `core:item.weapon.iron_sword` он берёт первый элемент `game.repository.list("item")` в каноническом порядке — это обращение к механизму, а не знание об игре. Если демо когда-нибудь потребует именно своих сущностей, `rh` заместит `core:module.debug.start` — модуль помечен `replaceable`.
- **Замороженный корпус не трогаем.** `Tests/Fixtures/PortableContentCore/valid/core` — фикстура, изображающая пакет по имени `core`, а не игру. Её ID остаются `core:*`, golden-прогон и его digest не меняются.
- **`rh` объявляет зависимость от `core`.** Порядок набора — `core`, затем `rh`; `core` всегда первый.

## Границы

Входят: пакет `rh` с манифестом, перевод боевых хостов на набор из двух пакетов, перенос 11 definitions с изменением namespace, перенос переводов и файлов ресурсов, развязка `core` от конкретных ID, гейт на отсутствие обратной ссылки, синхронизация документации.

Не входят:

- **Перенос механик.** `gameplay/root.lua`, `location_service.lua`, команды, валидаторы, события и экраны остаются в `core` — это прямое условие задачи.
- Реестр обработчиков команд и доменных методов: он понадобится, когда в `rh` поедет механика, а не сущности.
- Новые сущности и наполнение `rh` контентом сверх переносимого.
- UI управления модами и загрузка `rh` как отключаемого мода: `rh` — обязательная часть поставки игры.

## Milestones
 
- [x] M1 — [Package Setup](PackageSetup.md): пакет `rh` создан, все хосты грузят набор из двух пакетов.
- [x] M2 — [Entity Migration](EntityMigration.md): сущности, тексты, переводы и ресурсы переехали в `rh`.
- [x] M3 — [Core Decoupling](CoreDecoupling.md): `core` не содержит ни одной ссылки на `rh`, разделение закреплено гейтом и документацией.

## Критический путь

```text
Package Setup → Entity Migration → Core Decoupling
```

Этапы строго последовательны: перенос сущности в пакет, который не загружается, ломает боевую сессию, а развязка `core` бессмысленна до переноса.

M1 обязан завершиться зелёным при **пустом** `rh`: если набор из двух пакетов работает до переноса, любая поломка в M2 однозначно относится к переносу, а не к загрузке набора.

## Общие правила выполнения

1. Порядок набора всегда `core`, затем `rh`; `core` первый по правилу набора пакетов.
2. Ни одна задача не меняет замороженный корпус `Tests/Fixtures/PortableContentCore/` и golden-прогоны. Изменение их digest — признак ошибки в задаче, а не повод обновить pinned-значение.
3. Перенос ID выполняется `gv2-content rename`, а не текстовой заменой: инструмент проверяет `package_frozen` и находит обратные ссылки.
4. Каждая перенесённая сущность переносится целиком: definition, её текст, её ресурс и запись в PO-каталоге в одном change set.
5. `core` не получает ни одной ссылки на `rh:` ни в definitions, ни в Lua, ни в схемах.
6. Новое observable behavior синхронно отражается в contract.

## Итоговый Definition of Done

- [x] Оба хоста и CLI собирают репозиторий из `GameData/core` и `GameData/rh`.
- [x] Ни один `core:item.*`, `core:actor.*`, `core:location.*` не существует в `GameData/`.
- [x] Поиск `rh:` по `Scripts/`, `GameData/core/` и `Source/` не даёт совпадений; это проверяется гейтом.
- [x] Игра запускается, демо-экран отображается, переход между локациями работает.
- [x] Сейв, снятый до переноса, не заявляется совместимым: отсутствующий `core:location.*` даёт типизированную ошибку, а не тихий сбой.
- [x] Golden-прогон и его digest не изменились.
