---
title: Core Baseline Tasks
status: archived
version: 1.2
updated: 2026-08-20
depends_on:
  - ScalingModel.md
  - ../../../UI/ScreenTemplates.md
decisions:
  - ../../../ADR/0013-unified-text-pipeline.md
  - ../../../ADR/0017-centralized-ui-presentation-paths.md
---

# M3 — Core Baseline

> **Материализует:** разделы 4—6 [Core UI Baseline](../../../Proposals/CoreUIBaselineAndScalingProposal.md).
> **Задачи:** UIF-11…16.
> **Результат:** из базового набора можно собрать экран сложнее текстового блока с кнопками.

## Результат этапа

Появляются элементы, без которых невозможны ни вкладки, ни списки персонажей, ни поведение на 720p. Семь существующих виджетов перестают быть неуправляемыми данными.

Каждый элемент вводится вместе с потребителем; элементы без потребителя вынесены в [Core UI Extended Element Set](../../../Proposals/CoreUiExtendedElementSetProposal.md).

## Задачи

- [x] **UIF-11 — Три слоя владения композитами**
  - Зависимости: UIF-01.
  - Предложение исходно знало только `core` и игру; у проекта три слоя.
  - Done: contract фиксирует, что примитивы и конвейеры принадлежат `core`, композиты любой текстовой игры — `textsystem`, композиты конкретной игры — `rh`; критерий трёх вопросов применяется до реализации композита; переиспользуемость внутри слоя не поднимает композит выше; правило записано так же, как для сущностей.
  - Evidence: `Docs/UI/README.md`, `Docs/Architecture/Modding.md`.

- [x] **UIF-12 — Контейнер и область прокрутки**
  - Зависимости: UIF-08, UIF-11.
  - Прокрутка не косметика: без неё требование 720p невыполнимо, потому что не помещающийся контент становится недостижимым.
  - Done: созданы `Panel` (`WBP_Panel`, `BackgroundBorder`+`ContentSlot`) и `ScrollArea` (`WBP_ScrollArea`, `ScrollBox`+`ContentSlot`) с явными политиками масштабирования своих визуальных примитивов; прокрутка остаётся UI-local и в сейв не попадает; ассеты созданы через `unreal-mcp` (`CreateWidgetBlueprint`+`AddWidget`+`CompileWidgetBlueprint`+`save_assets`); оба элемента входят в контрактный automation-инвентарь.
  - Evidence: `Content/UI/Widgets/WBP_Panel.uasset`, `Content/UI/Widgets/WBP_ScrollArea.uasset`, `Source/GV2/Private/UI/`, `GV2.Runtime.UIKit.CentralThemeAndComponents`.

- [x] **UIF-13 — Обобщённый список**
  - Зависимости: UIF-12, UIF-03.
  - Сегодня повторяемый контент существует только как `ButtonList` — частный случай, элемент которого может быть только кнопкой.
  - Done: реализован обобщённый список с политикой раскладки (вертикальная, горизонтальная); элемент списка может быть композитом любого слоя; идентичность детей подчиняется правилу ключей M1 без исключений; `ButtonList` выражен через него с общей реализацией повторения; двумерное размещение в этот этап не входит; ассет `WBP_ListView` (`ContainerPanel`) создан через `unreal-mcp`.
  - Evidence: `Content/UI/Widgets/WBP_ListView.uasset`, `Docs/UI/ScreenTemplates.md`, `GV2.Runtime.UIKit.CentralThemeAndComponents`.

- [x] **UIF-14 — Контракты данных для существующих виджетов**
  - Зависимости: UIF-13.
  - `Image`, `ProgressBar` и `Portrait` существуют физически, но данными не управляются.
  - Done: опубликованы схемы Screen Field для изображения, индикатора и портрета; адаптеры выполняют обе детерминированные фазы наравне с существующими пятью; значения проходят через существующие leaf-адаптеры текста и изображения; ни один адаптер не заводит собственного пути разрешения ресурса.
  - Evidence: `Docs/UI/ScreenTemplates.md`, `Source/GV2/Private/UI/`.

- [x] **UIF-15 — Модалка и иконка**
  - Зависимости: UIF-14.
  - Done: `Modal` получает контракт Screen Field и участвует в слое `modal_stack`; добавлена `Icon` как примитив с политикой `PreserveAspect` (`WBP_Icon`, создан дублированием `WBP_Image` и перепривязкой к `UGV2IconWidgetBase` через `unreal-mcp`); оба элемента используют существующие конвейеры; сырые строки и прямая работа с brush отклоняются.
  - Evidence: `Content/TextSystem/UI/Widgets/WBP_Modal.uasset`, `Content/UI/Widgets/WBP_Icon.uasset`, `Docs/UI/ScreenTemplates.md`.

- [x] **UIF-16 — Соответствие конвейерам**
  - Зависимости: UIF-15.
  - Done: automation по полному инвентарю `/Game/UI` подтверждает, что ни один новый элемент не создаёт собственного пути для текста, изображения или повторяемых детей; одинаковый `resource_id` разрешается одинаково во всех элементах, включая состояние загрузки, отсутствующий ресурс и запасное представление; отрицательный случай воспроизводится на тестовом виджете и удаляется вместе с ним.
  - Evidence: `Source/GV2/Private/Tests/`, `GV2.Runtime.Presentation.*`.

## Проверка milestone

- [x] Из базового набора собирается экран со списком композитов внутри области прокрутки.
- [x] `Image`, `Portrait` и `Progress` управляются данными.
- [x] Список подчиняется правилу ключей.
- [x] Ни один элемент не обходит конвейеры текста и графики.
- [x] Элементов без потребителя не добавлено.
