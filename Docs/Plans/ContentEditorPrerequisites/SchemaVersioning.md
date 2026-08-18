---
title: Schema Versioning Tasks
status: normative
version: 1.0
updated: 2026-08-18
depends_on:
  - README.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
decisions:
  - ../../ADR/0029-content-authoring-and-schema-evolution.md
---

# M1 — Schema Versioning

> **Материализует:** [Definition Envelope and Schema Rules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md) в части изменения схем.
> **Задачи:** CEP-01…03.
> **Результат:** на вопрос «можно ли так менять схему» есть нормативный ответ.

## Результат этапа

Кнопка `+ Add Property` перестаёт быть способом сломать пакет одним кликом: у каждого класса изменения есть заранее известный ответ.

## Задачи

- [x] **CEP-01 — Создать ADR по авторингу контента**
  - Один ADR на три решения этапа: классификация схемных изменений, точечная правка вместо перезаписи файла, метаданные представления в отдельном файле.
  - Done: по каждому решению записана причина и отвергнутые альтернативы — разрешительная модель без версий, полная перезапись из разобранной модели, блок метаданных внутри схемы; отмечено, что сосуществование версий уже работает и потому строгое правило дёшево; отмечено, что схема влияет на `content_hash`, а метаданные обязаны быть от него изолированы; ADR принят до первой отметки `[x]` ниже.
  - Evidence: [ADR-0029](../../ADR/0029-content-authoring-and-schema-evolution.md).

- [x] **CEP-02 — Записать классификацию изменений**
  - Зависимости: CEP-01.
  - Done: [Definition Envelope and Schema Rules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md) содержит таблицу классов с ответом по каждому — добавление необязательного поля с default, добавление обязательного поля, сужение типа или диапазона, удаление поля, переименование; для каждого класса указано, что происходит с существующими definitions и с сейвами, ссылающимися на них; записано, что аддитивно-необязательное допустимо на месте, а остальное требует новой `schema_version` при сохранении старой до переезда контента; записано, что impact analysis для CLI — это вывод `gv2-content validate`, а не отдельный инструмент.
  - Evidence: `Docs/Architecture/DefinitionEnvelopeAndSchemaRules.md` (раздел «Schema evolution»).

- [x] **CEP-03 — Закрепить сосуществование версий фикстурой**
  - Зависимости: CEP-02.
  - Сосуществование работает сегодня, но ничем не защищено: регрессия сделала бы правило неисполнимым, а обнаружилась бы через месяц.
  - Done: замороженный корпус получает второй binding для одного `definition_type` и файл definitions на этой версии; conformance проверяет, что обе версии валидируются своими схемами и что конфликт двух bindings одной пары `(definition_type, schema_version)` остаётся fatal; negative case на конфликт; `repository_content_hash` корпуса обновлён осознанно, golden — воспроизведением манифеста.
  - Evidence: `Tests/Fixtures/PortableContentCore/valid/core/schemas/item_v2.schema.json5`, `definitions/items_v2.json5`, `RepresentativeCore.cpp`, `SchemaRegistryConformance.cpp` (`schema_registry.version_coexistence`, `schema_registry.multi_version_definitions_resolved`), `expected_core_content_hash.txt`, `expected_merged_content_hash.txt`, `golden_headless_10_seed_42.{manifest,digest}.json5`.

## Проверка milestone

- [x] На каждый класс изменения схемы contract даёт однозначный ответ.
- [x] Две версии схемы одного типа собираются вместе и покрыты фикстурой.
- [x] Конфликт bindings одной пары остаётся fatal.
