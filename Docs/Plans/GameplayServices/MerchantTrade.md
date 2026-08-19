---
title: Merchant Trade Tasks
status: normative
version: 1.0
updated: 2026-08-19
depends_on:
  - ServiceAuthoring.md
  - ../../Architecture/CanonicalStateAndSave.md
decisions:
  - ../../ADR/0026-core-and-gameplay-ownership.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M2 — Merchant Trade

> **Материализует:** раздел 15 [предложения](../../Proposals/GameplayServiceAuthoringProposal.md).
> **Задачи:** GSA-06…09.
> **Результат:** покупка перестаёт создавать предмет из воздуха: у товара есть владелец, который получает золото и теряет товар.

## Результат этапа

В `rh` появляется торговец. Меч и броня принадлежат ему, покупка проходит через `services.trade.buy`, и обе стороны меняются состоянием.

Этап обязателен: без него M1 остаётся механизмом без потребителя.

## Задачи

- [ ] **GSA-06 — Начальное состояние: игрок и торговец**
  - Зависимости: GSA-05, RAS-16 и RAS-05…07 плана [RHActorsSimplification](../RHActorsSimplification/README.md).
  - Определение `rh:actor.npc.merchant` существует, но экземпляра нет; экземпляр игрока сегодня создаётся только спеками через `game.instances.actors.create`, то есть у игры нет заводимого начального состояния.
  - Done: `rh` объявляет команду начала игры, создающую экземпляр игрока и экземпляр торговца через `instances.create`; торговец находится на рынке; торговцу принадлежат по одному экземпляру меча и брони; начальное золото сторон задано данными пакета, а не константами в коде ядра; `rh` не импортирует `instance_allocator` и не пишет в `game.state` напрямую; спеки `rh` переведены на эту команду вместо ручного создания актора.
  - Evidence: `GameData/rh/scripts/authoring/gameplay.lua`, `GameData/rh/definitions/`, `Tests/Lua/economy/`.

- [ ] **GSA-07 — Инвентарь как поведение одной сущности**
  - Зависимости: GSA-06.
  - Операции одной стороны имеют естественного владельца и остаются методами сущности; сервис владеет только координацией.
  - Done: в `rh` объявлены методы `Actor:require_item(item, opt_key)`, `Actor:take_item(item)` и `Actor:receive_item(instance)`; `require_item` даёт отказ до мутации, `take_item` снимает владение и возвращает экземпляр, `receive_item` назначает владение; существующий `Actor:add_item` либо выражен через `receive_item`, либо снят; ни одно из понятий не поднимается в `textsystem` или `core`.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua`, `Tests/Lua/economy/`.

- [ ] **GSA-08 — Сервис торговли и перевод покупки**
  - Зависимости: GSA-07.
  - Done: объявлен `rh:service.trade` с операцией `buy(buyer, seller, item)`; порядок — предусловия обеих сторон, затем мутации: покупатель платит, **продавец получает золото**, предмет меняет владельца; успешная покупка публикует факт события; `rh:command.buy` сведена к проверке места и вызову сервиса; действия `rh:action.buy_sword` и `rh:action.buy_armor` продолжают работать без изменения их идентификаторов; отсутствие товара у продавца даёт `rh:error.trade.item_not_available` до мутации; недостаток золота продолжает давать существующий отказ.
  - Evidence: `GameData/rh/scripts/authoring/gameplay.lua`, `GameData/rh/definitions/texts.json5`, `GameData/rh/localization/ru.po`.

- [ ] **GSA-09 — Спеки и сквозная верификация**
  - Зависимости: GSA-08.
  - Done: спека уровня FullGame покрывает успешную покупку с проверкой золота обеих сторон и смены владельца предмета, отказ по золоту, отказ по отсутствию товара и повторную покупку последнего экземпляра; спека уровня Core покрывает авторский слой сервисов без `rh`; одна spec suite исполняется обоими хостами, копия на C++ не создаётся; `repository_content_hash` и golden изменены осознанно и однократно, изменение воспроизведено манифестом; замороженный корпус не тронут; полный `ctest`, `gv2-headless --self-test`, `--check-scripts`, `validate_docs.py`, `validate_core_boundary.py` и Unreal automation зелёные; contracts и [Implementation Status](../../Status/ImplementationStatus.md) синхронизированы.
  - Evidence: `Tests/Lua/authoring/gameplay_services.lua`, `Tests/Lua/economy/`, отчёт CTest, golden-прогон.

## Проверка milestone

- [ ] Торговец существует в состоянии, находится на рынке и владеет мечом и бронёй.
- [ ] После покупки золото игрока уменьшилось ровно на цену, золото торговца увеличилось на неё же.
- [ ] Купленный предмет — тот же экземпляр, что принадлежал торговцу, а не новый.
- [ ] Покупка отсутствующего товара отклоняется до мутации.
- [ ] В `textsystem` и `core` не появилось понятий торговли, инвентаря и предметов.
