---
title: Presentation Model
status: informative
version: 1.3
updated: 2026-09-12
depends_on:
  - README.md
---

# Модель представления

> **Объясняет:** как желаемое состояние экрана превращается в виджеты и как нажатие возвращается в геймплей.
> **Нормативно:** [Screen Templates](../UI/ScreenTemplates.md), [Widget Registry](../UI/WidgetRegistry.md), [Semantic Input](../UI/SemanticInput.md).
> **Не является нормативным:** при расхождении прав contract.

Как желаемое состояние экрана превращается в конкретные виджеты и как нажатие возвращается обратно в геймплей.

## Разделение ответственности

```text
Lua                                    UE
────────────────────────────────       ────────────────────────────────
"нужен экран core:screen.test          Screen Registry: какой Blueprint
 с такими значениями полей"       →    Screen Template: раскладка
                                       Widgets: отрисовка, фокус, ховер
                                            ↓
"игрок выбрал это"               ←     Semantic Input: opaque handle
                                       превращается в command_id
```

Lua не описывает дерево виджетов и не знает про UMG. UE не принимает игровых решений. Между ними ходят только значения.

## Понятия

**Screen ID** — Stable ID экрана вида `core:screen.main`. Lua оперирует им, а не путём к Blueprint.

**Screen Template** — Widget Blueprint, сделанный в UMG-редакторе и унаследованный от общего базового экрана. Добавление нового экрана не требует C++.

**Screen Registry** — таблица соответствия `screen_id` → класс шаблона и слой. Проверяется при старте: неизвестный ID, дубликат или абстрактный класс не дают сессии стартовать.

**Screen Field** — именованное значение, которое Lua передаёт экрану: текст, список кнопок, картинка, поле ввода. Каждое поле имеет схему и обрабатывается своим адаптером.

**TextSpec** — текст в виде `text_id` плюс аргументы, а не готовая строка. Перевод подставляет presentation; Lua переведённого текста не видит и ветвиться по нему не может.

**Resource ID** — логическая ссылка на картинку или звук. Путь к ассету за границу не проходит.

**UI binding handle** — непрозрачный идентификатор, связывающий физическую кнопку с командой текущей ревизии интерфейса. Устаревший handle отклоняется, поэтому нажатие по исчезнувшему элементу не может выполнить чужую команду.

**Presentation source** — функция, зарегистрированная через `game.presentation.register_source(fn)`, которая перестраивает и публикует desired presentation автоматически при каждом изменении состояния, от которого она зависит ([ADR-0028](../ADR/0028-simplified-authoring-surface.md)). Автору не нужно вручную вызывать `show_screen` после каждой команды, меняющей то, что видно на экране — источник сам инвалидируется и пересчитывается. Пример — `GameData/textsystem/scripts/presentation/location_presenter.lua`: собирает экран локации из definition локации, текущего экрана и кнопок соседних локаций.

Нормативно: [Screen Templates](../UI/ScreenTemplates.md), [Widget Registry](../UI/WidgetRegistry.md), [Semantic Input](../UI/SemanticInput.md), [UI Document and Reconciliation](../UI/UIDocumentAndReconciliation.md), [Image Resources](../UI/ImageResources.md), [Presentation Snapshot and Effects](../UI/PresentationSnapshotAndEffects.md).

## Почему так

**Presentation восстановима.** Всё, что показано на экране, выводится из состояния и репозитория. Уничтожить и построить заново можно в любой момент, ничего игрового при этом не теряется.

**Blueprint не является источником истины.** Он не хранит игровых данных и не меняет состояние. Поэтому дизайнер может свободно менять раскладку, не рискуя геймплеем.

**Единые пути вместо локальных решений.** Текст, картинки, повторяющиеся элементы и ввод проходят через общие механизмы; отдельный виджет не заводит собственный способ отрисовать текст или обработать нажатие. Причина — [ADR-0017](../ADR/0017-centralized-ui-presentation-paths.md).

**Сессия публикуется целиком.** Пока новая сессия готовится, старая остаётся полностью рабочей. После необратимого начала замены старая VM уничтожается, а новая становится видимой только вместе со своим snapshot, bindings и первым успешно применённым документом. Initial document получает новый snapshot явно; он не выбирается через глобальный getter. Причина — [ADR-0044](../ADR/0044-session-replacement-and-registry-sealing.md).

## Что реализовано, а что нет

Работают: UI document с route/слоями/overlays/modals, реестр экранов с валидацией, набор базовых виджетов, универсальные Screen Fields, централизованная тема, текстовый конвейер, каталог изображений, semantic input с проверкой устаревших handle и source-based presentation с автоматической инвалидацией.

Не реализованы one-shot presentation effects и enter/exit animations. Полный replacement lifecycle и product save/load остаются незавершёнными; точные gaps — в [Implementation Status](../Status/ImplementationStatus.md). Это не отменяет уже работающую synchronous-реконсиляцию UI document.

## Дальше

- Что происходит до появления desired presentation — [GameplayModel](GameplayModel.md).
- Добавить экран — [Authoring/AddUIScreen](../Authoring/AddUIScreen.md).
