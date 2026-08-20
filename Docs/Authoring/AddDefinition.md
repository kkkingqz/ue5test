---
title: Add Definition
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# Добавить Definition

> **Помогает:** добавить content record существующего kind через schema-driven CLI.
> **Нормативно:** [Definition Envelope](../Architecture/DefinitionEnvelopeAndSchemaRules.md), [Stable ID](../Architecture/StableIDSpecification.md), [Repository](../Architecture/GameDataRepositoryContract.md).
> **Источник примера:** `GameData/rh/definitions/`, `Tools/Content/Source/Commands/`.

## Шаги

1. Посмотрите schema и authoring metadata:

   ```bash
   ./cmake-build-ci/Tools/Content/gv2-content describe GameData/rh item
   ```

2. Создайте запись с полным Stable ID:

   ```bash
   ./cmake-build-ci/Tools/Content/gv2-content new GameData/rh item rh:item.weapon.steel_sword
   ```

3. Заполните обязательные поля. Ссылки (`text_id`, `resource_ref`, `ref`) задаются полным `<namespace>:<kind>.<path>`.
4. Проверьте package:

   ```bash
   ./cmake-build-ci/Tools/Content/gv2-content validate GameData/rh
   ```

   Во время серии правок можно добавить `--watch`.
5. Если нужен переводимый текст, добавьте Definition kind `text` с `source_message`, затем запись в `<package>/localization/<locale>.po`.

## Не делайте так

- Не добавляйте неизвестное поле: Definition Envelope закрыт, сначала программист расширяет schema.
- Не изобретайте новый kind без schema и реального consumer-а.
- Не рассчитывайте на deep merge: override с тем же ID полностью заменяет запись.
- Не используйте Definition для runtime-state или поведения; Definition immutable после публикации snapshot.
