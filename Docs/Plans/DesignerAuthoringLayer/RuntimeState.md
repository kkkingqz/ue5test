---
title: Runtime State Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - Properties.md
  - ../../Architecture/CanonicalStateAndSave.md
---

# M4 — Runtime State

> **Материализует:** [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md) в части runtime-полей, привязанных к definition.
> **Задачи:** DLA-14…16.
> **Результат:** любой вид сущности получает runtime-поля без правок ядра; локация принадлежит персонажу.

## Результат этапа

`market.owner = aria` работает, потому что `owner` объявлен полем со `Storage: Runtime State`, а не потому что в ядре появилась секция для локаций.

Единственный этап плана, меняющий форму состояния. Обратная совместимость до релиза не поддерживается, поэтому миграции не пишутся — но `state_hash` и golden обновляются осознанно.

## Задачи

- [x] **DLA-14 — Универсальная секция runtime-состояния**
  - Зависимости: DLA-10.
  - Секция на каждый kind означала бы правку ядра при добавлении поля новому виду сущности — ровно то, чего слой избегает.
  - Done: одна секция, ключуемая по `definition_id`; список секций перестаёт быть литералом в `state_validator` и дополняется реестром, заполняемым пакетом на фазе `register` с freeze; ключ, не разрешающийся в существующий definition, отвергается по общему правилу «отсутствие Stable ID — всегда ошибка»; секция участвует в сохранении и загрузке, `state_hash` до и после совпадает.
  - Evidence: `Scripts/runtime/state_validator.lua`, `Scripts/runtime/save.lua`, `Scripts/runtime/load.lua`, `Scripts/runtime/migrate.lua`, `Tests/Lua/authoring/runtime_state.lua` (`universal_definition_state_section_in_canonical_state`).

- [x] **DLA-15 — Sparse-материализация**
  - Зависимости: DLA-14.
  - Done: чтение отсутствующего поля даёт schema default; любая запись материализует значение; снятие override выполняется явной операцией `reset()`; автоматической де-материализации нет; в contract записано, что схемный `Default` входит в определение значения, поэтому его изменение меняет наблюдаемое состояние всех несматериализованных сущностей, а два одинаково ведущих себя состояния могут иметь разный `state_hash`.
  - Evidence: `Scripts/authoring/properties.lua` (`wrap_definition`, `methods.reset`, `register_definition_type`), `Scripts/authoring/context.lua` (`mod.def`, `mod.location`), `Tests/Lua/authoring/runtime_state.lua` (`sparse_materialization_and_reset`, `definition_wrapper_decorator_and_context_access`).

- [x] **DLA-16 — Локация как свойство персонажа**
  - Зависимости: DLA-13, DLA-14.
  - Done: единственное хранимое поле — `current_location` актора, объявленное как `ref_definition<location>`; `world.current_location` остаётся **read-only аксессором** и в состоянии не хранится; `state.world.current_location_id` удалён, а не оставлен дублирующим; переход между локациями и его валидатор работают на новом поле; golden обновлён воспроизведением манифеста.
  - Evidence: `Scripts/runtime/world.lua` (`WorldLocationReadOnly`), `Scripts/gameplay/location_service.lua`, `GameData/rh/scripts/gameplay/`, `Tests/Lua/world/`, `Tests/Lua/authoring/runtime_state.lua` (`location_as_actor_property_and_readonly_world`).

## Проверка milestone

- [x] Runtime-поле добавляется новому виду сущности без правок ядра.
- [x] Несматериализованное поле не занимает места в сейве и читается как default.
- [x] `reset()` снимает override, повторная запись материализует снова.
- [x] В состоянии нет двух представлений локации персонажа.
- [x] `state_hash` до и после сохранения совпадает.
