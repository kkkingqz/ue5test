---
title: Gameplay Events and World Implementation Plan
status: archived
version: 3.0
updated: 2026-08-15
depends_on:
  - ../../../Architecture/CommandsAndEvents.md
  - ../../../Architecture/CanonicalStateAndSave.md
  - ../../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../../ADR/0003-command-and-event-model.md
  - ../../../ADR/0004-lua-state-mutation.md
---

# План реализации Gameplay Events and World

> **Архив.** План выполнен полностью (M1–M5) и больше не является источником задач. Нормативное поведение перенесено в [Commands and Events](../../../Architecture/CommandsAndEvents.md), [Lua Runtime Contract](../../../Architecture/LuaRuntimeContract.md) и [Canonical State and Save](../../../Architecture/CanonicalStateAndSave.md). Документ сохраняется как implementation record.

## Цель

Замкнуть command path: validators до мутации, факты после commit, подписка на факты и первый доменный объект мира. План материализует [Commands and Events](../../../Architecture/CommandsAndEvents.md); новых архитектурных решений он не вводит и ADR не требует.

## Ведущий сценарий

План строится вокруг одного сценария, а не вокруг подсистем: **игрок перемещается между локациями, мир фиксирует переход, подписчик на это реагирует**. Каждый вводимый механизм получает потребителя в день появления. Это половина vertical slice acceptance из [Architecture Overview](../../../Architecture/Overview.md) — без save/load.

## Состояние на входе

M1 и M2 выполнены: реестр валидаторов с порядком и семантикой отказа, доменный объект `game.instances.world` и текущая локация. Реализованы canonical state, instance identity, mutation window, ActorRegistry и реестр Gameplay Services.

Событий нет, фазы не выставляются, подписки нет. Команда перемещения не реализована.

Пауза, введённая после M2, снята задачей `TAS-15`. Её причина устранена: проверки Lua-правил больше не требуют C++, а тесты не зависят от контента игры. Реализация GEW-04/GEW-05 обошлась примерно в 1000 строк C++ тестовой обвязки — эти два набора уже мигрированы в спеки и удалены.

## Принятые решения

- **Подписка по `event_id`.** Условие проверяется внутри Lua-обработчика. Декларативных фильтров и предикатного языка не вводится: фильтр на равенство ключей может появиться позже без слома этой модели, но только под измеренную стоимость фан-аута.
- **Мир живёт под `game.instances.world`.** Фасад остаётся закрытым: мир — singleton runtime instance, а не новое поле верхнего уровня.
- **События не входят в run digest.** Следствие: порядок доставки и порядок подписчиков проверяются спеками, а не регрессионной сеткой golden. Digest всё равно изменится, потому что меняется state (текущая локация) — это достаточно для регрессий состояния, но не для регрессий порядка событий.
- **Проверки пишутся Lua-спеками** в `Tests/Lua/` (ADR-0024). Новый C++ conformance entry point на правило, выраженное в Lua, запрещён и ломает CI.

## Границы

Входят: command validators, конверт и шина событий, детерминированная доставка, подписка, доменный объект мира, команда перемещения целиком.

Не входят: save/load и фаза `Saving`; декларативные фильтры подписки; моды; локализация; presentation-эффекты; события в digest.

## Milestones

- [x] M1 — [Command Validators](CommandValidators.md): проверка намерения до первой мутации.
- [x] M2 — [World Domain Object](WorldDomainObject.md): `game.instances.world` и первое содержимое мира.
- [x] M3 — [Event Bus Core](EventBusCore.md): факты после commit и детерминированная доставка.
- [x] M4 — [Subscription and Reaction](SubscriptionAndReaction.md): подписка и права обработчика.
- [x] M5 — [Travel Slice](TravelSlice.md): сценарий целиком.

## Критический путь

```text
Command Validators
→ World Domain Object
→ Event Bus Core
→ Subscription and Reaction
→ Travel Slice
```

M1 и M2 независимы друг от друга и могут идти параллельно; оба нужны M5. M3 обязан предшествовать M4.

## Общие правила выполнения

1. Валидатор не меняет state и не открывает mutation window.
2. Обработчик события не меняет state: во время pump окно закрыто. Он может поставить команду в очередь, и она откроет собственное окно позже.
3. Порядок доставки и порядок подписчиков детерминированы и проверяются Lua-спеками, исполняемыми обоими host-ами.
4. Спека, которой нужна собственная регистрация валидаторов, подписок или команд, исполняется на fixture-сессии; спека над продакшн-сессией открывает mutation window явно.
5. Каждая задача добавляет negative case, если меняет failure semantics.
6. Новое observable behavior синхронно отражается в contract.

## Итоговый Definition of Done

- [x] Отказ валидатора оставляет state неизменным и не публикует фактов.
- [x] Факты доставляются только после успешного commit.
- [x] Порядок доставки и подписчиков воспроизводим и одинаков в обоих host-ах.
- [x] Превышение лимита pump переводит session в `Failed`.
- [x] Обработчик события не может изменить state напрямую.
- [x] Сценарий перемещения проходит целиком в UE и headless.
