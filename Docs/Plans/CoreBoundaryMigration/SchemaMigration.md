---
title: Schema Migration Tasks
status: draft
version: 1.0
updated: 2026-08-17
depends_on:
  - InstanceExtension.md
  - ../../Architecture/StableIDSpecification.md
---

# M3 — Schema Migration

> **Материализует:** [ADR-0026](../../ADR/0026-core-and-gameplay-ownership.md) в части владения схемами.
> **Задачи:** CBM-09…13.
> **Результат:** предметная модель игры описана её пакетом, ядро описывает только то, что требует runtime.

## Результат этапа

`item_v1` и `location_v1` уезжают в `rh` целиком. `actor_v1` в ядре сокращается до `discriminator` — единственного поля, без которого runtime не соберёт инстанс; остальные поля актора описывает пакет.

Самый дорогой этап плана: затрагивает реестр ссылочных полей состояния, боевые фикстуры и UE-провайдер.

## Задачи

- [x] **CBM-09 — Разрешить пакету привязывать схему существующего kind**
  - Правило принято [ADR-0026](../../ADR/0026-core-and-gameplay-ownership.md) и записано в contracts; здесь оно реализуется.
  - Done: пакет объявляет binding для kind, объявленного ядром, если ядро для него binding не объявляет; перекрытие существующего binding отклоняется отдельной диагностикой; конфликт двух bindings одной пары `(definition_type, schema_version)` остаётся fatal; kind без binding ни в одном пакете набора отклоняется при использовании, а не молча пропускается.
  - Evidence: `SchemaRegistry.cpp` и `PackageDiscovery.cpp` разрешают привязку схем набором пакетов. `gv2-content describe GameData/rh item` успешно находит схему `rh:schema.definition.item.v1`.

- [x] **CBM-10 — Реестр ссылочных полей состояния**
  - `Scripts/runtime/state_validator.lua` содержит `DEFINITION_REFERENCE_FIELDS = { current_location_id = "location" }` — поле игры внутри общего механизма.
  - Done: реестр заполняется регистрацией на фазе `register` с freeze; ядро не содержит ни одной записи об игровых полях; `rh` регистрирует `current_location_id`; загрузка сейва (`core:module.runtime.load`) использует тот же реестр при переписывании редиректов; спека на регистрацию и на отказ после freeze.
  - Evidence: `Scripts/runtime/state_validator.lua` реализует `register_reference_field` и `freeze_reference_fields`. `GameData/rh/scripts/gameplay/travel.lua` регистрирует `current_location_id`. Спеки в `Tests/Lua/resources/state_reference_fields.lua` проверяют регистрацию, freeze, валидацию и rewrite редиректов.

- [x] **CBM-11 — Перенести схемы `item` и `location`**
  - Зависимости: CBM-09.
  - Done: `rh:schema.definition.item.v1` и `rh:schema.definition.location.v1` описаны пакетом; `GameData/core/schemas/item_v1.schema.json5` и `location_v1.schema.json5` удалены; semantic validator цены переезжает вместе со схемой предмета; `gv2-content describe GameData/rh item` показывает схему из `rh`; `validate` зелёный.
  - Evidence: Созданы `GameData/rh/schemas/item_v1.schema.json5` и `location_v1.schema.json5`, удалены из `GameData/core/schemas/`. `gv2-content validate GameData/{core,rh,sample}` возвращает `ok`.

- [x] **CBM-12 — Урезать `actor_v1` до `discriminator`**
  - Зависимости: CBM-07, CBM-09.
  - Done: схема ядра содержит только `discriminator` и объяснение, почему именно это поле требуется runtime; `base_hp`, `name_text_id` и прочие поля актора описаны пакетом собственной схемой либо extension-блоком; выбор обёртки по discriminator продолжает работать; актор без зарегистрированного decorator обрабатывается по правилу CBM-06.
  - Evidence: `GameData/core/schemas/actor_v1.schema.json5` сокращён до `discriminator`. Создан `GameData/rh/schemas/actor_rh_extension_v1.schema.json5`, поля актора в `GameData/rh/definitions/actors.json5` перенесены в `extensions.rh`.

- [x] **CBM-13 — Синхронизировать contract и фикстуры**
  - Зависимости: CBM-10–CBM-12.
  - Done: [Definition Envelope and Schema Rules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md) и [GameDataRepository Contract](../../Architecture/GameDataRepositoryContract.md) описывают владение схемой по набору пакетов; [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md) описывает реестр ссылочных полей; примеры в документации обновлены на схемы `rh`; замороженный корпус не изменён.
  - Evidence: Обновлены `DefinitionEnvelopeAndSchemaRules.md`, `CanonicalStateAndSave.md`, `GameDataRepositoryContract.md`. `validate_docs.py` — 161/161 файлов валидны.

## Проверка milestone

- [x] `GameData/core/schemas/` содержит только framework-схемы.
- [x] Схема актора в ядре — одно поле.
- [x] `state_validator.lua` не знает ни одного игрового поля.
- [x] `repository_content_hash` golden не изменился (`0e68f173...`).
- [x] Слайс TestGameplaySlice проходится целиком.
