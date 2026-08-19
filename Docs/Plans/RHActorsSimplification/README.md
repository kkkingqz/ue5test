---
title: RH Actors Lua Simplification Implementation Plan
status: normative
version: 1.1
updated: 2026-08-19
depends_on:
  - ../../Proposals/RHActorsLuaSimplificationProposal.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0027-designer-lua-authoring-layer.md
  - ../../ADR/0028-simplified-authoring-surface.md
  - ../../ADR/0031-entity-authoring-extensions.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# План реализации RH Actors Lua Simplification

> **Материализует:** [RH Actors Lua Simplification Proposal](../../Proposals/RHActorsLuaSimplificationProposal.md) и [ADR-0032](../../ADR/0032-field-contracts-and-generic-instance-creation.md).
> **Задачи:** RAS-01…17.
> **Результат:** декларативные контракты полей сущностей (`field.*`), разделение структурных инвариантов и геймплейных предусловий, обобщённое создание экземпляров (`instances.create`), чистое доменное описание `rh/scripts/gameplay/actors.lua` без низкоуровневых утечек runtime.

## Цель

Довести файл `GameData/rh/scripts/gameplay/actors.lua` до чистого декларативного описания доменной семантики RH:
1. Описание полей состояния через дескрипторы контрактов (`field.non_negative_integer()`).
2. Централизованная валидация инвариантов полей в точке записи вместо ручных проверок `validate_amount` в каждом методе.
3. Разделение структурных инвариантов состояния и геймплейных предусловий (`require_*`).
4. Инкапсуляция создания динамических сущностей (инвентарь/предметы) через фасад `instances.create()` без прямого импорта `instance_allocator` и обращения к `game.state`.
5. Устранение неявного fallback `self -> player`.

## Состояние на входе

| Что | Было | Станет |
|---|---|---|
| Описание полей актора | Вспомогательная таблица `RESOURCES` с избыточными параметрами | Декларативные дескрипторы `Actor.gold = field.non_negative_integer()` |
| Проверка типов и диапазонов | Ручная функция `validate_amount()` дублируется во всех методах | Автоматическая валидация схемы при любой записи в `ActorWrapper.__newindex` |
| Проверка underflow в `spend_*` | Повторная ручная проверка `if current < amount` | Защита инварианта `gold >= 0` на уровне контракта поля; явный `require_*` до вызова `spend_*` |
| Получатель `self` в методах | Неявный fallback `self or player` | Строгая работа с получателем `self` без обращения к глобальному игроку |
| Создание предметов (`add_item`) | Прямой импорт `instance_allocator` и запись в `game.state.item_instances` | Обобщённый фасад `instances.create("item", { definition = ..., owner = self })` |

## Принятые решения

Зафиксированы в [ADR-0032](../../ADR/0032-field-contracts-and-generic-instance-creation.md) и уточнены по итогам разбора предложения.

- **Схема сущности является композицией, а не цепочкой перекрытия.** Поля из generic entity kind, `definition_id` и `discriminator` сливаются по имени поля; более конкретный источник переопределяет одноимённое поле и не скрывает остальные.
- **Повторное объявление поля запрещено** (`FieldAlreadyDeclared`); осознанное переопределение — явным флагом `override`, по образцу [ADR-0025](../../ADR/0025-lua-module-replacement-and-export-freezing.md).
- **Объявление поля не регистрирует вид актора.** `ActorTypeNotRegistered` остаётся привязанной к `discriminator`.
- **Ядро не перечисляет виды экземпляров.** Вид объявляется пакетом; имя секции состояния выводится из имени вида. `actor` — осознанное исключение.
- **Срабатывание контракта поля — runtime fault, а не отказ команды**, и состояние при нём не откатывается. Транзакционность окна мутации вынесена в отдельное [предложение](../../Proposals/MutationWindowTransactionalityProposal.md).
- **Контракт поля не проверяет аргументы методов.** Проверка принадлежит валидаторам команд; до их появления — локальная проверка в `rh`.

## Milestones

- [x] **M1 — [Field Contracts](FieldContracts.md)**: дескрипторы полей `field.*` (`core:module.authoring.field`), интеграция с авторскими прототипами `_ENV`, композиция схем, запрет повторного объявления, сохранение проверки вида актора. RAS-01…04, RAS-13…15.
- [ ] **M2 — [Generic Instance Creation](GenericInstanceCreation.md)**: обобщённый фасад создания экземпляров `instances.create()`, канонизация дескрипторов/ссылок, реестр видов экземпляров без единого имени вида в ядре, изоляция `instance_allocator` и коллекций состояния. RAS-05…07, RAS-16.
- [ ] **M3 — [RH Actors Migration](RhActorsMigration.md)**: рефакторинг `GameData/rh/scripts/gameplay/actors.lua`, устранение `RESOURCES`, `validate_amount` и `instance_allocator`, чистые методы сущности `Actor`, локальная проверка аргументов. RAS-08…10, RAS-17.
- [ ] **M4 — [Validation and Specs](ValidationAndSpecs.md)**: спеки валидации инвариантов полей, геймплейных отказов, создания экземпляров и кросс-хостового паритета. RAS-11…12.

## Критический путь

```text
M1 (Field Contracts) ──► M2 (Generic Instances) ──► M3 (RH Migration) ──► M4 (Specs & Validation)
```

## Общие правила выполнения

1. Никакие runtime-функции, дескрипторы или метатаблицы не попадают в save container ([INV-001](../../Architecture/Invariants.md), [INV-008](../../Architecture/Invariants.md)).
2. Мутация канонического состояния внутри методов сущностей разрешена только во время исполнения команд в открытом окне мутации ([INV-003](../../Architecture/Invariants.md)).
3. Абстракции `Resource` и `Item/Inventory` не переносятся в `TextSystem` и остаются семантикой `rh` ([ADR-0030](../../ADR/0030-textsystem-layer-and-data-driven-package-set.md), [ADR-0032](../../ADR/0032-field-contracts-and-generic-instance-creation.md)).
4. В golden-прогоне меняются только `script_set_hash` и производный `digest_hash`: правка скриптов неизбежно меняет их обоих. Изменение `repository_content_hash`, хэша состояния, `final_screen_id` или `final_screen_fields` — признак ошибки, а не ожидаемое следствие рефакторинга.
5. Нарушение контракта поля является runtime fault: команда прерывается, а сделанные до ошибки записи в каноническом состоянии остаются. Ни одна задача плана не полагается на контракт поля как на средство сохранения целостности состояния.
6. Ни одна задача не оставляет `spend_*` и `add_*` без проверки аргумента: удаление `validate_amount` компенсируется локальной проверкой в `rh` до появления валидаторов команд.
7. Ни одна задача не добавляет в ядро имя вида экземпляра, поля или сущности конкретной игры ([INV-016](../../Architecture/Invariants.md)).

## Итоговый Definition of Done

- [ ] `Actor.gold` и `Actor.stamina` объявлены через `field.non_negative_integer()`.
- [ ] Попытка записать в `actor.gold` отрицательное, дробное или строковое значение отклоняется ошибкой валидации поля.
- [ ] `require_gold` и `require_stamina` возвращают геймплейный отказ `fail()` до мутации при недостатке ресурса.
- [ ] `spend_gold` и `spend_stamina` не содержат дублирующих проверок `validate_amount` и underflow.
- [ ] `Actor:get_gold()` и `Actor:get_stamina()` работают строго с `self`.
- [ ] `Actor:add_item()` использует `instances.create("item", ...)` без прямого импорта `instance_allocator` и обращения к `game.state`.
- [ ] Из `GameData/rh/scripts/gameplay/actors.lua` полностью удалены `RESOURCES` и `validate_amount`.
- [ ] Поле, объявленное на `Actor`, действует и для актора, имеющего схему по `discriminator`.
- [ ] Повторное объявление поля без `override` отклоняется `FieldAlreadyDeclared`.
- [ ] Неизвестный `discriminator` отклоняется независимо от объявленных полей.
- [ ] В ядре нет ни одного имени вида экземпляра; неизвестный вид — типизированный отказ.
- [ ] `spend_gold(-10)` отклоняется проверкой аргумента в `rh`, а не проходит молча.
- [ ] Спеки всех уровней (`Core`, `TextSystem`, `FullGame`) и replay golden-run проходят на 100% на обоих хостах.
