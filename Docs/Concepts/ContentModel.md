---
title: Content Model
status: informative
version: 1.2
updated: 2026-08-18
depends_on:
  - README.md
---

# Модель контента

> **Объясняет:** как файлы автора превращаются в данные, которые читает геймплей.
> **Нормативно:** [GameDataRepository](../Architecture/GameDataRepositoryContract.md), [Definition Envelope](../Architecture/DefinitionEnvelopeAndSchemaRules.md), [Stable ID](../Architecture/StableIDSpecification.md), [Modding](../Architecture/Modding.md).
> **Не является нормативным:** при расхождении прав contract.

Как файлы, которые пишет автор, превращаются в то, что читает геймплей. Нормативные правила — в contracts по ссылкам.

## Путь контента

```text
GameData/                    контейнер пакетов (core, textsystem, rh, sample)
  mods.lock.json5              generated: набор, версии, fingerprints
  <package_id>/
    definitions/*.json5        что есть в игре/слое
    schemas/*.json5            какие поля допустимы
    localization/*.po          переводы
    package.json5              манифест: package_id, namespace, version,
                                compatibility, dependencies, redirects, tombstones
        ↓
  discovery                    сканирование контейнера, граф зависимостей,
                                топологический порядок, сверка с mods.lock.json5
        ↓
  BuildRepository()            парсинг, схемы, ссылки, override
        ↓
  immutable snapshot           неизменяемый, с каноническим хэшем
        ↓
  pinned read handle           сессия держит его до перезапуска
        ↓
  game.repository              Lua читает detached-копии
```

## Понятия

**Package** — каталог с обязательным манифестом `package.json5` (`package_id`, `namespace` — обязано совпадать с `package_id`, `version`; опционально `compatibility` и `dependencies`). Identity не выводится из имени каталога — без манифеста пакет не собирается вообще. Базовый набор поставки состоит из трёх слоёв: `GameData/core` (движок: framework-схемы, базовые экраны, тексты, ресурсы), `GameData/textsystem` (текстовый движок: схемы локаций, действия переходов, презентер экрана локации) и `GameData/rh` (игра: схемы предметов/акторов, конкретные предметы, локации, персонажи, ресурсы и тексты). Движок не знает сущностей и предметных схем игры, а слои расширяют систему строго снизу вверх. `GameData/sample` — демо-контент для проверок и примеров, не часть боевой поставки.

**`mods.lock.json5`** — generated-файл в корне контейнера: зафиксированный набор пакетов, их версии и fingerprints, сверяемые при каждой сборке. Не редактируется вручную и не является манифестом отдельного пакета.

**Definition** — запись в `definitions/*.json5`. Имеет Stable ID, тип и поле `data`, устройство которого задаёт схема.

**Schema** — самоописывающийся файл в `schemas/`: объявляет `definition_type`, версию и допустимые поля с их типами и ограничениями. Схемы читает и рантайм, и инструменты — поэтому `gv2-content describe GameData/rh <тип>` показывает поля, не заглядывая в документацию. Framework-схемы объявляет `core`, текстовые схемы — `textsystem`, а предметные схемы игры — пакет `rh`.

**Typed reference** — поле, значение которого является Stable ID другого definition: `ref` с ожидаемым kind, `text_id`, `resource_ref`. Опечатка в ссылке — фатальная ошибка сборки, а не проблема, найденная в рантайме.

**Snapshot** — результат успешной сборки: неизменяемый набор победивших definitions с индексами, provenance и каноническим хэшем.

**Override** — если два пакета объявляют один ID, побеждает последний по порядку загрузки, и он заменяет запись целиком. Слияния полей нет.

**Redirect и tombstone** — способ переименовать или снять с публикации ID, не ломая сейвы и ссылки. Объявляются в `package.json5`.

Нормативно: [GameDataRepository Contract](../Architecture/GameDataRepositoryContract.md) — сборка, разрешение, override, redirects, API чтения; [Definition Envelope and Schema Rules](../Architecture/DefinitionEnvelopeAndSchemaRules.md) — конверт файла, типы полей, значения по умолчанию, расширения; [Stable ID Specification](../Architecture/StableIDSpecification.md) — грамматика, владение namespace, жизненный цикл идентификаторов; [Modding](../Architecture/Modding.md) — манифест, discovery контейнера, порядок загрузки, `mods.lock.json5`.

## Что важно понимать автору

**Данные и правила разделены.** Definition описывает, каким предмет является. Что происходит при его использовании, описывает Lua. Декларативного языка триггеров и эффектов в JSON5 нет и не планируется: если нужного поля в схеме нет, поле добавляет программист, а не автор обходит это Lua-кодом внутри данных.

**Сборка целиком или никак.** Одна ошибка в одном файле не даёт опубликоваться всему снимку. Это сделано намеренно: наполовину загруженный контент хуже отсутствующего.

**Сессия видит неизменный снимок.** Публикация нового снимка не влияет на уже идущую сессию — она продолжает работать со своим до перезапуска.

**Изменение данных не ломает сейвы.** Сейв ссылается на ID, а не хранит копию definition. Поменять цену предмета можно свободно; ошибкой является только исчезновение ID, на который сейв ссылается.

**Тексты живут в двух местах по-разному.** Definition kind `text` хранит идентичность и исходную строку; переводы лежат в PO-каталогах и в хэш контента не входят, поэтому правка перевода ничего не инвалидирует. Причины — [ADR-0022](../ADR/0022-external-translation-catalog.md).

## Инструменты автора

`gv2-content describe` — какие поля есть у типа; `new` — валидная заготовка; `validate` (в том числе `--watch`) — проверка; `refs` и `rename` — обратные ссылки и переименование до публикации; `index` — все ID для автодополнения; `coverage` — полнота перевода. Полный список и коды возврата — [Build and Tooling](../Architecture/BuildAndTooling.md).

## Дальше

- Как definition превращается в экземпляр — [RuntimeInstances](RuntimeInstances.md).
- Добавить свой Definition — [Authoring/AddDefinition](../Authoring/AddDefinition.md).
