---
title: Properties Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - Foundation.md
  - ../../../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# M3 — Properties

> **Материализует:** [Definition Envelope and Schema Rules](../../../Architecture/DefinitionEnvelopeAndSchemaRules.md) в части хранилища и политики записи.
> **Задачи:** DLA-10…13.
> **Результат:** поле знает, где хранится и как в него писать; ссылка знает, definition это или экземпляр.

## Результат этапа

Дизайнер пишет `aria.morale = 40` и получает схемную проверку, а `aria.gold = 100` — отказ с указанием штатной операции.

## Задачи

- [x] **DLA-10 — Хранилище и политика записи в схеме**
  - Зависимости: DLA-01.
  - Done: поле схемы объявляет `Storage` (`Definition` | `Runtime State`) и `WritePolicy` (`ReadOnly` | `Plain` | `Managed`); значения по умолчанию выбраны так, что существующие схемы остаются валидными; `gv2-content describe` показывает оба атрибута; неизвестное значение — ошибка схемы, а не молчаливый дефолт.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/FieldValidation.h`, `Source/GV2ContentCore/Private/FieldValidation.cpp`, `Source/GV2ContentCore/Private/ScalarValidation.cpp`, `Tools/Content/Source/Commands/DescribeCommand.cpp`, `Tools/Content/test_authoring_tools.py` (тест 35).

- [x] **DLA-11 — Authoring-обёртка поля**
  - Зависимости: DLA-10.
  - Схемная проверка принадлежит обёртке, а не окну мутации: окно guard-ит `game.state` целиком и о схемах не знает.
  - Done: чтение definition-поля работает, запись отвергается с `Cannot modify definition field`; запись plain-поля проверяется по схеме (тип, nullable, kind ссылки, min/max, enum) **до** изменения состояния, и при отказе `write_revision` не растёт; коллекция возвращает обёртку с `add`/`remove`, проверяющую элемент; в contract записано, что прямая запись программиста в `game.state` минует схемную проверку, но не минует разрешение и счётчик.
  - Evidence: `Scripts/authoring/properties.lua`, `Scripts/runtime/actor_registry.lua`, спека `Tests/Lua/authoring/properties.lua` (`definition_field_read_succeeds_and_write_is_rejected`, `plain_field_validates_schema_before_mutation`, `two_reference_types_and_collection_wrappers`).

- [x] **DLA-12 — Managed-поля и штатные операции**
  - Зависимости: DLA-11.
  - Done: схема объявляет `Operations` для managed-поля; прямое присваивание отвергается сообщением, называющим объявленные операции; на фазе freeze рантайм проверяет, что каждая объявленная операция существует на обёртке соответствующего discriminator, и отвергает сессию иначе; negative case на несуществующую операцию.
  - Evidence: `Scripts/runtime/actor_registry.lua`, `Scripts/authoring/properties.lua`, спека `Tests/Lua/authoring/properties.lua` (`managed_field_rejects_direct_assignment_with_domain_operations_hint`, `missing_domain_operation_on_freeze_raises_error`).

- [x] **DLA-13 — Два типа ссылок**
  - Зависимости: DLA-10.
  - Done: схема различает `ref_definition<kind>` и `ref_instance<kind>`; первая хранит canonical ID и возвращает definition-обёртку, вторая хранит `kind@N` и возвращает свежую runtime-обёртку; проверка ссылочной целостности при удалении сущности становится реестро-ориентированной вместо ручного обхода `item_instances` и квестов в `actor_registry`; удаление сущности с живой `ref_instance` на неё отвергается с указанием держателя ссылки.
  - Evidence: `Scripts/authoring/properties.lua`, `Scripts/runtime/actor_registry.lua`, спека `Tests/Lua/authoring/properties.lua` (`two_reference_types_and_collection_wrappers`, `referential_integrity_on_actor_removal`).

## Проверка milestone

- [x] Запись в definition-поле отвергается.
- [x] Отклонённая запись plain-поля не меняет ни состояние, ни `write_revision`.
- [x] Отказ managed-поля называет операцию из схемы.
- [x] Объявленная операция, которой нет на обёртке, не даёт сессии стартовать.
- [x] Удаление сущности с живой ссылкой на неё отвергается.
