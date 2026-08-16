---
title: Setup and Workflow
status: informative
version: 1.1
updated: 2026-08-16
depends_on:
  - README.md
---

# Где что лежит и как работать

> **Задача:** понять устройство папок, научиться проверять свою работу и наладить рабочий цикл.
> **Нужно:** собранный `gv2-content` (попросите программиста собрать один раз).
> **Проверка:** `gv2-content validate GameData/rh` выводит `ok`.

## Папки

```text
GameData/
  rh/                          ← ваша игра
    package.json5              служебный файл, не трогаем
    definitions/               здесь вы работаете
      actors.json5             персонажи
      items.json5              предметы
      locations.json5          локации
      texts.json5              тексты
      resources.json5          картинки
    localization/
      ru.po                    переводы

  core/                        ← движок, правит программист
    schemas/                   описание того, какие поля бывают у объектов
    definitions/screens.json5  экраны

Resources/
  rh/                          файлы картинок вашей игры
```

Имена файлов внутри `definitions/` роли не играют — важно поле `type` внутри файла. Разложение «один тип на файл» просто удобно. Можно и `weapons.json5` отдельно от `armor.json5`, оба с `type: "item"`.

## Формат файлов

Формат называется JSON5. От обычного JSON отличается тем, что прощает человеческие привычки:

- имена полей без кавычек: `price: 10`, а не `"price": 10`;
- комментарии через `//`;
- запятая после последнего элемента списка не считается ошибкой.

Текст всегда в кодировке UTF-8 — любой нормальный редактор так и сохраняет.

## Главная команда

```bash
gv2-content validate GameData/rh
```

Она читает весь контент, проверяет его и отвечает одной строкой. Успех выглядит так:

```text
ok content_hash=bfc32d539907b516c1d6ce3a48098b85a596eebb4560e298d1d1c8104467c5ee
```

`content_hash` — отпечаток текущего состояния контента, вам он не нужен.

Ошибка называет файл, строку, колонку и объект:

```text
error core:diagnostic.schema.value.missing_required_field definitions/items.json5:7:13
  Required object field is absent (definition=rh:item.weapon.iron_sword) (pointer=/definitions/0/data/price)
```

Читается так: в файле `definitions/items.json5`, строка 7 — у предмета `rh:item.weapon.iron_sword` не хватает обязательного поля `price`.

**Проверка всегда идёт целиком.** Если хоть один объект неверен, контент не собирается вовсе — игра не запустится с частично валидным набором. Это специально: лучше увидеть ошибку сразу, чем поймать её в игре через час.

## Рабочий цикл

Запустите проверку в режиме слежения и оставьте окно открытым:

```bash
gv2-content validate GameData/rh --watch
```

Теперь после каждого сохранения файла вы сразу видите `ok` или ошибку. Правьте — смотрите — правьте. Так ошибка находится за секунды, а не после запуска игры.

## Полезные команды

**Посмотреть, какие поля бывают у объекта:**

```bash
gv2-content describe GameData/rh item
```

```text
definition_type: item
schema_id: core:schema.definition.item.v1
package: core (schemas/item_v1.schema.json5)
fields:
  label_text_id: text_id (required)
  price: int64 (required, min=0)
  icon_resource_id: resource_ref (required, resource_class=texture_2d)
```

Строка `package:` показывает, что описание полей взято из `core` — так и должно быть, поля объектов задаёт движок.

**Завести новый объект заготовкой:**

```bash
gv2-content new GameData/rh item rh:item.weapon.axe
```

Команда допишет в `definitions/items.json5` запись со всеми обязательными полями и предсказуемыми именами ссылок:

```json5
{
  id: "rh:item.weapon.axe",
  data: {
    label_text_id: "rh:text.item.weapon.axe.label",
    price: 0,
    icon_resource_id: "rh:resource.item.weapon.axe.icon",
  },
  tags: [],
  deprecated: false,
  extensions: {},
},
```

Заготовка сразу не проходит проверку — она ссылается на текст и картинку, которых ещё нет. Это не ошибка команды, а список того, что осталось сделать: `validate` назовёт обе недостающие ссылки поимённо.

**Посмотреть один объект целиком:**

```bash
gv2-content inspect GameData/rh rh:item.weapon.iron_sword
```

**Найти, кто ссылается на объект** — обязательно перед удалением или переименованием:

```bash
gv2-content refs GameData/rh rh:item.weapon.iron_sword
```

**Переименовать объект по всему контенту** — сам находит и правит все ссылки, руками так делать не надо:

```bash
gv2-content rename GameData/rh rh:item.weapon.iron_sword rh:item.weapon.steel_sword
```

**Проверить полноту перевода:**

```bash
gv2-content coverage GameData/rh --locale=ru
```

## Про пути в командах

Во всех командах указывайте `GameData/rh` — свой пакет. Описания полей лежат в `core`, но инструменты находят их сами: они видят, что `rh` зависит от `core`, и заглядывают туда. Указывать два пути не нужно.
