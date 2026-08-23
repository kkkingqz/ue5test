---
title: ToolingAndWidgetHygiene Archive Summary
status: archived
version: 1.0
updated: 2026-08-23
---

# ToolingAndWidgetHygiene: итог выполнения

> **Материализует:** исторический итог выполненного плана; документ не является источником правил или задач.

## Цель и результат

**Цель:** устранить findings [Plan Audit Findings](../../Status/Archive/PlanAudit2026-08.md), каждый из которых ограничен одним файлом или одним инструментом и не требует изменений через границы слоёв.

**Результат:** незащищённые правила получили гейты (индекс планов, псевдолокаль, отклонение контейнера для `set`, структура `main.cpp`, реляционные инварианты констант раскладки), два дефекта раскрывающегося списка устранены (магический ключ заголовка, беспричинное закрытие при повторном применении неизменной модели), пакет-образец `sample` хранит состояние в каноническом `game.state` вместо локальных переменных модуля, единый физический размер шрифта между виджетами проверяется тестом.

## Задачи

### M1 — Gates

Правила, которые раньше держались на внимательности, стали держаться на проверке. Каждая задача добавила один кейс CTest либо привела документ в соответствие с фактом.

- `TWH-01` — Индекс активных планов приведён в соответствие каталогу
- `TWH-02` — Псевдолокаль проверяется кейсом CTest
- `TWH-03` — Возвращена проверка отклонения контейнера для `set`
- `TWH-04` — Структурный гейт вместо числовой границы `main.cpp`
- `TWH-05` — Гейт констант раскладки через реляционные инварианты

### M2 — Widgets and Sample

Раскрывающийся список перестал зависеть от строкового литерала и закрываться без причины; пакет-образец хранит состояние там же, где его хранит игра.

- `TWH-06` — Заголовок списка перестал опознаваться по имени ключа
- `TWH-07` — Список не закрывается при неизменной модели
- `TWH-08` — Пакет `sample` хранит состояние канонически
- `TWH-09` — Один стиль — один физический размер во всех виджетах

## Проверка

Полный регрессионный прогон на итоговом коммите: 66/66 портативных CTest, 81/81 UE automation тестов (включая `GV2.Runtime.UIKit.LayoutConstantsRelationalInvariants`, `GV2.Runtime.UIKit.DropdownSelectWidgetContract`, `GV2.Runtime.UIKit.WidgetSemanticFontSizeContract`, `GV2.Runtime.Lua.SpecRunnerHost`), `gv2-headless --self-test`, `gv2-headless --check-scripts` и `validate_docs.py` — без ошибок. Все восемь закрытых findings (`UIF-AF-01`, `UIF-AF-03`, `CCM-AF-01`, `CCM-AF-02`, `EXT-AF-01`, `EXT-AF-02`, `EXT-AF-04`, `UIF-AF-07`) отмечены в [AuditFindings](../../Status/Archive/PlanAudit2026-08.md) со ссылкой на задачу, закрывшую каждый.

## Актуальные нормативные источники

- [BuildAndTooling](../../Architecture/BuildAndTooling.md)
- [WidgetRegistry](../../UI/WidgetRegistry.md)
- [ImageResources](../../UI/ImageResources.md)
- [AuditFindings](../../Status/Archive/PlanAudit2026-08.md)

## Полная история

`source_commit`: [82352e8fdc6f85f37c128fbc20a8410c66002a4e](https://github.com/kkkingqz/ue5test/commit/82352e8fdc6f85f37c128fbc20a8410c66002a4e)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/82352e8fdc6f85f37c128fbc20a8410c66002a4e/Docs/Plans/ToolingAndWidgetHygiene) содержит исходные task-файлы, acceptance criteria и evidence.
