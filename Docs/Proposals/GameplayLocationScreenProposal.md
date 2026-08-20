---
title: Gameplay Location Screen Proposal
status: draft
proposal_state: accepted_for_planning
version: 1.0
updated: 2026-08-20
depends_on:
  - CoreUIBaselineAndScalingProposal.md
  - ../UI/README.md
  - ../UI/ScreenTemplates.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../UI/ImageResources.md
decisions:
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0030-textsystem-layer-and-data-driven-package-set.md
---

# Предложение по первому реальному игровому экрану

> **Предлагает:** шаблон `LocationScreen` в слое `textsystem` — модель экрана из четырёх композитов, наполняемая значениями определения локации и текущего состояния.
> **Затрагивает:** [UI Index](../UI/README.md), [Screen Templates](../UI/ScreenTemplates.md), [UI Document](../UI/UIDocumentAndReconciliation.md), [Image Resources](../UI/ImageResources.md).
> **Не является нормативным:** до реализации действует текущий contract.

## 1. Цель

Создать первый полноценный игровой экран, пригодный не только как UI test fixture, но и как основа реального gameplay.

Рабочее имя:

```text
LocationScreen
```

Экран должен одновременно:

1. быть usable как основной экран локации;
2. проверять принятый Core UI baseline;
3. использовать единые text/graphics pipelines;
4. работать через существующую presentation/semantic-input architecture;
5. использовать gameplay-specific composites поверх Core UI;
6. корректно работать от design resolution 4K до 720p;
7. адаптироваться к изменению aspect ratio без uniform bitmap scaling всего экрана.

---

# 2. Главный принцип

`LocationScreen` состоит из двух вещей: **модели экрана** и **набора значений**.

```text
модель экрана          набор значений
(шаблон и поля)   ←    (определение локации + текущее состояние)
```

Модель — один Screen Template с фиксированным набором полей и четырьмя композитами. Значения — то, чем эти поля наполняются для конкретной локации в конкретный момент.

Экран является **одним Screen Instance** в маршруте. Деления на «постоянную оболочку» и «контекстную часть» на уровне слоёв Game Shell нет: постоянство верхней панели и панели персонажа обеспечивается не отдельным слоем, а переиспользованием экземпляра при реконсиляции.

Отсюда требование, без которого постоянство не работает: **переход между локациями не меняет `screen_id` и `instance_key` маршрута.** Совпадение обоих означает, что виджет переиспользуется и обновляются только изменившиеся поля; смена любого из них пересоздала бы весь экран, включая панели, которые обязаны пережить переход.

Фон является собственным фоном конкретного виджета сцены, а не слоем `background` оболочки. Слой `background` оболочки остаётся глобальной подложкой и этим экраном не используется.

```text
              Модель
        ┌────────┴────────┐
     TopBar          StatusPanel
     SceneView       CommandPanel
```

# 3. Владение

Владение следует действующему разделению из [UI Index](../UI/README.md), а не бинарному «ядро и игра».

**`LocationScreen` уже назван там шаблоном слоя `textsystem`.** Туда же попадают все четыре композита: верхняя панель со днём и локацией, панель состояния персонажа, сцена «фон плюс персонаж» и панель доступных действий — это основа любой текстовой RPG, а не правила Red Hood.

| Слой | Что принадлежит | Каталог |
|---|---|---|
| `core` | Примитивы `Text`, `Image`, `Button`, `Icon`, `ProgressBar`, `Separator`, контейнеры `Panel`, `ScrollArea`, `ListView`, конвейеры текста и изображений | `Content/UI/` |
| `textsystem` | Композиты `RichText`, `ButtonList`, `Portrait`, `Modal` и шаблоны экранов, включая `LocationScreen` и четыре его блока | `Content/TextSystem/UI/` |
| `rh` | Определения локаций, экранов, текстов, ресурсов и действий | `GameData/rh/` |

Практическое следствие: **этот экран не создаёт ни одного ассета в `Content/RH/UI/`.** Игра участвует только данными. Если бы композиты попали в `rh`, вторая текстовая игра переписала бы все четыре.

Игре принадлежит смысл содержимого: что ресурсом является золото, как считается день, какие действия доступны в таверне.

# 4. Общий layout

```text
┌────────────────────────────────────────────────────────────────────┐
│                         GAME TOP BAR                               │
│ Day 17      Tavern                     Gold 120       [!] [●]      │
├───────────────────────────────────────────────────┬────────────────┤
│                                                   │                │
│                                                   │ PLAYER STATUS  │
│                                                   │                │
│                                                   │   portrait     │
│                   SCENE VIEW                      │   name         │
│                                                   │   resources    │
│          background image                         │   equipment    │
│                 +                                 │   statuses     │
│          character overlay                        │                │
│                                                   │                │
│                                                   │                │
├───────────────────────────────────────────────────┴────────────────┤
│                        COMMAND PANEL                               │
│ [Talk] [Trade] [Look Around] [Leave] [Inventory]                  │
└────────────────────────────────────────────────────────────────────┘
```

---

# 5. Screen composition

```text
WBP_LocationScreen
│
├── WBP_GameTopBar
├── WBP_SceneView
├── WBP_PlayerStatusPanel
└── WBP_CommandPanel
```

Все четыре блока являются gameplay composites, собранными из Core UI primitives.

---

# 6. GameTopBar

## 6.1. Назначение

Показывает небольшое количество постоянно полезной глобальной информации.

Baseline:

```text
game day
current location
major resource(s), например gold
important notification indicators
```

Пример:

```text
Day 17       Tavern                         Gold 120    [!] [●]
```

## 6.2. Не перегружать TopBar

TopBar не должен превращаться в полную character sheet/status dashboard.

Не помещать туда:

```text
полный список effects
полный equipment
длинные quest descriptions
большое количество character stats
```

## 6.3. Composition

```text
WBP_GameTopBar
├── Panel
├── Text / RichText
├── Icon
└── Separator
```

---

# 7. PlayerStatusPanel

## 7.1. Назначение

Правый постоянный блок показывает состояние player character.

Минимально:

```text
portrait
name
important resource meters
active equipment/items
important temporary statuses/effects
```

Пример:

```text
┌──────────────────────┐
│                      │
│      PORTRAIT        │
│                      │
├──────────────────────┤
│ Aria                 │
│                      │
│ Stamina ██████░░     │
│ Health  ████████     │
│                      │
│ Equipment            │
│ [Sword] [Torch]      │
│                      │
│ Effects              │
│ [Poison] [Blessing]  │
└──────────────────────┘
```

## 7.2. Equipment и Status

В presentation они могут выглядеть похоже как icon collections, но gameplay semantics остаются различными:

```text
equipment/items
temporary statuses/effects
```

UI не объединяет их в один gameplay concept.

## 7.3. Подсказки

Всплывающие подсказки для иконок предметов и эффектов **в первый экран не входят**. Механизм подсказок остаётся в отложенном наборе элементов и вводится вместе со своим потребителем отдельно.

До этого момента иконка несёт только изображение; текстовое пояснение при необходимости выражается подписью.

## 7.4. Composition

```text
WBP_PlayerStatusPanel
├── Panel
├── Portrait
├── Text / RichText
├── Meter
├── Icon list / Repeater
└── Text подписи
```

---

# 8. SceneView

## 8.1. Назначение

`SceneView` — gameplay composite для отображения текущей 2D сцены.

Core не получает специализированный primitive:

```text
LocationBackgroundWithCharacter
```

Core предоставляет нейтральные:

```text
Image
Overlay/layout
Panel
```

а gameplay package собирает:

```text
WBP_SceneView
```

## 8.2. Минимальная структура

```text
SceneView
├── Background Layer
├── Character Layer
└── Optional ContextText
```

## 8.3. Background

Фон использует `PreserveAspect` **без обрезки**. Новых режимов и под-опций не вводится: действующий [Image Resource Contract](../UI/ImageResources.md) запрещает crop и непропорциональное растягивание для `fixed_aspect`, и это ограничение сохраняется.

Отсюда следует композиция фона из двух слоёв внутри сцены:

```text
подложка      tile        заполняет прямоугольник сцены при любых пропорциях
иллюстрация   fixed_aspect вписывается целиком, пропорции сохранены
```

Иллюстрация вписывается (`contain`) и центрируется; незакрытую часть прямоугольника закрывает бесшовная подложка. Полноэкранная иллюстрация без подложки является ошибкой композиции — то же правило, что уже действует для фонового слоя оболочки.

## 8.4. Character image

Персонаж отображается как 2D image с alpha channel.

Минимальный behavior:

```text
PreserveAspect
fit = Contain
anchor = bottom
```

Presentation задаёт только:

```text
resource_id
visibility
```

Множитель масштаба через границу **не передаётся**: это физический параметр отрисовки, а такие параметры boundary не пересекают. Размер персонажа определяется правилом `contain` в прямоугольнике слоя персонажей и привязкой к нижнему краю; ни состояние, ни презентация пиксельных величин не хранят.

Горизонтальное размещение в первом срезе не параметризуется: персонаж один и центрируется. Оно появится вместе с несколькими персонажами, и тогда будет выражено именованными позициями, а не числами.

---

# 9. Scene layers

SceneView рекомендуется сразу сделать небольшим фиксированным layered composite:

```text
Background
Characters
Foreground
UI Overlay
```

Conceptually:

```text
Layer 0  background
Layer 1  character images
Layer 2  optional foreground art
Layer 3  optional scene-local UI overlay
```

Это не general-purpose scene graph.

На первом этапе не вводится arbitrary Lua-controlled layout tree.

Цель — без архитектурной переделки позже поддержать:

```text
несколько персонажей
foreground objects
simple scene markers
```

---

# 10. Multiple characters

Первый vertical slice может поддерживать одного visible character.

Но `Character Layer` проектируется как repeated collection для будущего:

```text
0..N character presentation entries
```

Каждый entry может иметь:

```text
resource
anchor
scale
order
visibility
```

Без gameplay decisions внутри UE.

---

# 11. Optional ContextText

SceneView может иметь optional короткий contextual text block.

Пример:

```text
Aria watches you carefully.
```

Подходит для:

```text
короткого описания сцены
результата действия
короткой реплики
context hint
```

Он не заменяет полноценный Dialogue/LongText screen.

---

# 12. CommandPanel

## 12.1. Назначение

Нижний блок показывает доступные в текущем контексте semantic gameplay actions.

Пример:

```text
[Talk] [Trade] [Look Around] [Leave] [Inventory]
```

## 12.2. Command presentation model

Command button желательно описывать как:

```text
label
optional icon
enabled
semantic action/binding
```

Conceptually:

```text
CommandPresentation
    label
    icon?
    enabled
    action
```

UI не содержит gameplay decisions.

## 12.3. Repeater

`CommandPanel` использует Core Generic List/Repeater mechanism, а не fixed набор кнопок.

```text
CommandPanel
    └── Repeater<CommandButton>
```

Это позволяет динамически менять доступный набор действий.

## 12.4. Responsive command layout

На большой ширине:

```text
[Talk] [Trade] [Drink] [Leave] [Inventory] [Rest]
```

На меньшей:

```text
[Talk] [Trade] [Drink]
[Leave] [Inventory] [Rest]
```

Предпочтительно wrap/reflow вместо сильного уменьшения текста.

---

# 13. Persistent vs Context-dependent data

## Persistent

```text
day
current location name
major resources
player portrait
important player meters
equipment/active items
important statuses
```

## Context-dependent

```text
scene background
visible scene characters
scene context text
available commands
```

Это distinction относится к presentation composition, а не обязательно к разным gameplay state stores.

---

# 14. Желаемая презентация в терминах документа

Lua описывает желаемое состояние экрана, а не управляет виджетами. Описание выражается **в терминах принятой модели документа**, а не собственным форматом.

`LocationScreen` — один Screen Instance маршрута с фиксированным набором полей:

```json5
route: {
  instance_key: "location",
  screen_id: "textsystem:screen.location",
  fields: {
    top_bar:       { schema_id: "…ui_field.top_bar.v1",       value: { … } },
    player_status: { schema_id: "…ui_field.player_status.v1", value: { … } },
    scene:         { schema_id: "…ui_field.scene.v1",         value: { … } },
    commands:      { schema_id: "…ui_field.command_list.v1",  value: { … } },
  },
}
```

Каждое поле имеет схему и адаптер наравне с существующими полями экрана. Повторяемые элементы внутри полей — команды, иконки предметов и эффектов — подчиняются общему правилу ключей без исключений.

## 14.1. Откуда берутся значения

Набор значений собирается из двух источников, и различие между ними существеннее, чем деление на постоянное и контекстное.

| Источник | Что даёт |
|---|---|
| Определение экрана, выбранное через `screen_ids` локации | Заголовок, описание, ресурс фона, персонаж сцены, набор действий |
| Текущее каноническое состояние | День, золото, имя локации, портрет, показатели, предметы, эффекты, доступность команд |

`screen_ids` сохраняется и определяет **чем именно наполняется** экран для данной локации. Модель экрана при этом одна: три определения экрана `rh` остаются тремя наборами значений одного шаблона.

Презентер `textsystem` объединяет оба источника в поля документа. Игра не пишет полей документа напрямую.

## 14.2. Расширение схемы определения экрана

Существующая схема определения экрана (`title_text_id`, `description_text_id`, `include_connected_locations`, `actions[]`) не содержит ни фона, ни персонажа.

Добавлять их в схему `core` неверно: фон сцены и персонаж — понятия текстовой RPG, а не механизм движка. Расширение объявляется **отдельной схемой слоя `textsystem`** через существующий механизм extension site — тем же способом, которым `textsystem` и `rh` уже расширяют определение актора.

# 15. Semantic input

CommandPanel не вызывает gameplay logic напрямую.

```text
Button
    ↓
Semantic Action
    ↓
Command
    ↓
Lua gameplay
```

Widget отвечает только за presentation/input state:

```text
label
enabled
focus
hover
```

Gameplay Command остаётся Lua-authoritative.

---

# 16. Design resolution

Reference resolution:

```text
3840 × 2160
16:9
```

Предварительные proportions:

```text
TopBar
    ~7–8% height

PlayerStatusPanel
    ~20–24% width

CommandPanel
    ~17–20% height

SceneView
    remaining flexible area
```

Эти числа — **ориентир на опорном разрешении и только на нём**, а не правило раскладки.

Действующее правило одно и записано в contract: preferred-размер с ограничениями min/max, распределение фактического viewport, никакого равномерного масштабирования кадра. Доля экрана в процентах — мышление от равномерного масштаба: на 21:9 панель в 22 % ширины стала бы заметно шире без всякой причины.

---

# 17. Responsive sizing rules

Preferred behavior:

```text
TopBar
    semantic/DPI-aware preferred height

PlayerStatusPanel
    preferred width + min/max constraints

CommandPanel
    content-driven/preferred height + min/max

SceneView
    Fill remaining space
```

UI не масштабирует весь 4K layout как одну bitmap-like surface.

---

# 18. 720p target

Экран должен оставаться usable на:

```text
1280 × 720
```

При уменьшении viewport предпочтение отдаётся:

```text
reflow
wrap
scroll
min/max sizing
reduction of optional spacing
```

а не уменьшению текста ниже readable minimum.

На 720p должны оставаться доступны:

```text
TopBar critical info
PlayerStatusPanel critical info
SceneView
all current commands
```

---

# 19. Aspect ratio behavior

Для первого этапа обязательны:

```text
16:9
21:9
```

На ultrawide дополнительную ширину прежде всего получает:

```text
SceneView
```

PlayerStatusPanel сохраняет preferred/min/max width.

Сложная breakpoint system не вводится без реальной необходимости.

---

# 19a. Отсутствующие ресурсы

Локация может не иметь фона, персонаж — изображения, предмет — иконки. Действующая политика различает обязательный ресурс, который блокирует применение, и необязательный, который заменяется совместимой заглушкой.

Для этого экрана вводятся **ресурсы-заглушки**, и при отсутствии правильного показывается заглушка соответствующего режима отрисовки:

| Что отсутствует | Поведение |
|---|---|
| Фоновая иллюстрация | Показывается только бесшовная подложка |
| Изображение персонажа | Заглушка-силуэт; слой персонажа не исчезает |
| Иконка предмета или эффекта | Заглушка-иконка; элемент списка остаётся на месте |
| Портрет игрока | Заглушка-портрет |

Заглушки являются обычными ресурсами и разрешаются тем же конвейером. Отсутствие ресурса не делает экран непригодным и не прерывает применение документа; оно остаётся видимым в диагностике.

# 20. Graphics scaling

Пример policies:

```text
Scene background
    tile-подложка + PreserveAspect (contain)

Character image
    PreserveAspect + Contain

Portrait
    PreserveAspect

Icons
    PreserveAspect

Panels/buttons
    NineSlice

Simple fill backgrounds
    FreeStretch
```

Все policies используют Core graphics pipeline.

Gameplay widgets не реализуют собственный asset scaling mechanism.

---

# 21. DPI-aware text

Все текстовые части:

```text
TopBar
PlayerStatusPanel
ContextText
Command buttons
```

используют общий Core DPI-aware text pipeline.

Gameplay screen не задаёт resolution-specific font sizes.

Он использует semantic styles:

```text
topbar
body
character_name
meter_label
command
context_text
```

---

# 22. Core UI integration coverage

Реализация LocationScreen должна использовать и тем самым practically проверить:

```text
Text
RichText
Image
Portrait
Icon
Panel
Meter
Repeater/List
Button
Separator
```

а также:

```text
Text Pipeline
Graphics/Resource Pipeline
DPI scaling
FreeStretch
NineSlice
PreserveAspect
Semantic Input
Screen Field adapters
```

Таким образом экран становится первым настоящим integration fixture Core UI.

---

# 23. Не добавлять gameplay-specific primitives в Core

Не следует создавать Core widgets:

```text
NPCPortrait
PlayerStatus
LocationScene
CommandBar
EquipmentStatus
```

Это gameplay composites.

Core содержит только нейтральные reusable primitives.

---

# 24. Первый implementation slice

Минимально полезная версия:

```text
LocationScreen
├── TopBar
│   ├── Day
│   ├── Location
│   └── Gold
│
├── SceneView
│   ├── Background
│   └── One Character
│
├── PlayerStatusPanel
│   ├── Portrait
│   ├── Name
│   ├── Stamina Meter
│   └── Active Item Icons
│
└── CommandPanel
    └── Dynamic command buttons
```

Optional ContextText может быть включён в тот же slice, если RichText pipeline уже стабилен.

---

# 25. Второй implementation slice

После первого vertical slice:

```text
multiple visible characters
status/effect icons
подсказки для иконок
health/additional meters
foreground layer
notification icons
command icons
responsive wrap
```

---

# 26. Не входит в первый экран

На первом этапе не требуется:

```text
dialogue system
inventory grid
character sheet
quest log
drag-and-drop
arbitrary scene graph
visual screen editor
complex responsive breakpoint framework
animated 2D skeletal characters
3D viewport
```

---

# 27. Testing matrix

Минимально:

```text
3840×2160 16:9
2560×1440 16:9
1920×1080 16:9
1280×720 16:9
3440×1440 21:9
2560×1080 21:9
```

Проверить:

```text
TopBar readability
PlayerStatusPanel bounds
Scene background crop
character aspect preservation
command wrap/reflow
DPI text consistency
all commands reachable
no critical clipping
```

---

# 28. Gameplay integration test

Минимальный gameplay scenario:

```text
Player находится в Tavern.

TopBar:
    Day 17
    Tavern
    Gold 120

PlayerStatus:
    portrait
    stamina 15/20
    Sword active

Scene:
    tavern background
    Aria visible

Commands:
    Talk
    Trade
    Leave
```

После:

```text
Command: travel → Market
```

desired presentation меняется:

```text
TopBar.location
Scene.background
Scene.characters
CommandPanel.entries
```

Persistent player block может остаться тем же, кроме изменившихся gameplay values.

---

# 29. Architectural acceptance criteria

Proposal считается реализованным, когда:

1. существует рабочий `LocationScreen`;
2. он собран из Core UI primitives и gameplay composites;
3. gameplay-specific composites не перенесены в Core;
4. TopBar показывает persistent global information;
5. PlayerStatusPanel показывает portrait/resources/items/statuses;
6. SceneView поддерживает background + alpha character overlay;
7. SceneView использует layered composition;
8. CommandPanel строится динамически через Repeater/List;
9. command buttons отправляют Semantic Actions, а не выполняют gameplay logic;
10. graphics используют Core resource/scaling pipeline;
11. text использует общий Core DPI-aware pipeline;
12. фон сохраняет пропорции без обрезки: подложка плюс вписанная иллюстрация;
13. character image сохраняет aspect ratio;
14. экран usable на 1280×720;
15. экран корректно работает на 16:9 и 21:9;
16. SceneView получает основную дополнительную ширину на ultrawide;
17. UI не использует whole-screen uniform bitmap scaling;
18. экран служит integration test Core UI baseline.

---

# 30. Итоговая структура

```text
Gameplay Package
│
├── WBP_GameTopBar
├── WBP_PlayerStatusPanel
├── WBP_SceneView
├── WBP_CommandPanel
│
└── WBP_LocationScreen
        │
        ├── persistent presentation
        │   ├── TopBar
        │   └── PlayerStatusPanel
        │
        └── contextual presentation
            ├── SceneView
            └── CommandPanel
```

Ни один из этих blocks не владеет gameplay state.

```text
Lua canonical state
      ↓
desired presentation
      ↓
LocationScreen composites
      ↓
Core UI primitives
      ↓
UMG/CommonUI rendering
```

Input:

```text
Command Button
      ↓
Semantic Action
      ↓
Lua Command
      ↓
gameplay mutation
      ↓
new desired presentation
```

---

# 31. Главный принцип

> **Первый gameplay screen должен быть одновременно реальным игровым экраном и интеграционным доказательством UI architecture: Core владеет primitives и rendering pipelines, gameplay package владеет композициями и смыслом отображаемых данных.**
