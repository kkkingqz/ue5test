---
title: Add Definition
status: informative
version: 1.1
updated: 2026-08-15
depends_on:
  - README.md
---

# Добавить definition

> **Задача:** добавить предмет, локацию, актора, экран, текст или ресурс.
> **Нужно:** собранный `gv2-content`; понимание [ContentModel](../Concepts/ContentModel.md).
> **Нормативно:** [Definition Envelope](../Architecture/DefinitionEnvelopeAndSchemaRules.md), [Stable ID](../Architecture/StableIDSpecification.md), [GameDataRepository](../Architecture/GameDataRepositoryContract.md).

## Шаги

**1. Узнать, какие поля есть у типа.**

```bash
./cmake-build-ci/Tools/Content/gv2-content describe GameData/core item
```

Вывод порождается из схемы, поэтому он всегда актуален.

**2. Создать заготовку.**

```bash
./cmake-build-ci/Tools/Content/gv2-content new GameData/core item core:item.weapon.steel_sword
```

Команда создаст запись с обязательными полями в `definitions/items.json5`, проверив грамматику ID и отсутствие дубликата.

**3. Заполнить значения.** Ссылки на другие definitions пишутся полным Stable ID: `text_id` для названий, `resource_ref` для иконок.

**4. Проверить.**

```bash
./cmake-build-ci/Tools/Content/gv2-content validate GameData/core
```

Для непрерывной работы удобнее `validate GameData/core --watch`.

**5. Добавить текст, если он нужен.** Название и описание — это отдельные definitions kind `text` с полем `source_message`. Переводы кладутся в `GameData/core/localization/<locale>.po` и на хэш контента не влияют.

## Проверка результата

`validate` завершается с кодом 0. Если предмет должен быть виден в игре, это отдельный шаг: definition сам по себе ничего не отображает, его должен запросить геймплей или экран.

## Типичные ошибки

**Опечатка в `text_id` или `resource_ref`.** Даёт фатальную ошибку сборки с указанием файла и позиции — это норма, а не проблема инструмента: ссылки проверяются до публикации.

**Новый kind без схемы.** Kind берётся из реестра, а не придумывается: `core:npc.aria` невалиден, потому что kind `npc` не зарегистрирован. Персонаж — это `core:actor.*`.

**Правка нового поля без правки схемы.** Конверт закрыт: поле, которого нет в схеме, отвергается. Сначала поле добавляется в схему.

**Попытка описать поведение в данных.** Декларативных триггеров и эффектов нет. Если поведение не выражается существующими полями, нужное поле добавляет программист — см. [AddCommand](AddCommand.md) и [AddLuaModule](AddLuaModule.md).
