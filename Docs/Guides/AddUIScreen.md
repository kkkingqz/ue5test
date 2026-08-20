---
title: Add UI Screen
status: informative
version: 1.2
updated: 2026-08-20
depends_on:
  - README.md
---

# Добавить экран

> **Задача:** добавить экран и научить Lua его запрашивать.
> **Предмет:** Screen Definition, template registry и desired presentation.
> **Нужно:** Unreal Editor для шагов с Blueprint; понимание [PresentationModel](../Concepts/PresentationModel.md).
> **Нормативно:** [Screen Templates](../UI/ScreenTemplates.md), [Widget Registry](../UI/WidgetRegistry.md), [Semantic Input](../UI/SemanticInput.md).

## Шаги

**1. Завести definition экрана.** Kind `screen`, например `core:screen.shop` — см. [AddDefinition](AddDefinition.md).

**2. Создать Widget Blueprint** в редакторе, унаследовав его от общего базового экрана. Раскладка, анимации и локальные визуальные состояния — здесь; игровых данных Blueprint не хранит.

**3. Зарегистрировать экран** в реестре: соответствие `screen_id` → класс шаблона и слой. Неизвестный ID, дубликат или абстрактный класс не дадут сессии стартовать — это проверяется при загрузке, а не при первом показе.

**4. Собрать желаемое состояние в Lua.** Экран запрашивается по `screen_id` с набором Screen Fields; каждое поле имеет схему и обрабатывается своим адаптером. Тексты передаются как `TextSpec` (`text_id` плюс аргументы), картинки — как `resource_id`.

**5. Привязать действия.** Кнопка получает binding с `command_id`; нажатие возвращается semantic input-ом с непрозрачным handle. Экран не передаёт `command_id` из Blueprint напрямую.

**6. Проверить.** Presentation-путь проверяется Unreal automation: `GV2.Runtime.Presentation.*`. Игровая часть — командой и её спекой, см. [AddCommand](AddCommand.md).

## Типичные ошибки

**Путь к Blueprint в Lua.** Lua оперирует `screen_id`; путь к ассету за границу не проходит.

**Готовая строка вместо `TextSpec`.** Тогда текст нельзя перевести, а Lua начинает зависеть от локали.

**Собственный способ отрисовать текст или картинку внутри виджета.** Тексты, изображения, повторяющиеся элементы и ввод идут по общим путям; локальная альтернатива запрещена.

**Изменение состояния из Blueprint.** Presentation игровых решений не принимает: любое изменение проходит через команду.

## Ограничение сегодня

UI-документ с маршрутами, слоями и модальными окнами не реализован: активен ровно один экран за раз. Экран заменяется целиком. Актуальный объём — [Implementation Status](../Status/ImplementationStatus.md).
