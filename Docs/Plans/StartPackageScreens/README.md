---
title: Start Package Screens Implementation Plan
status: active
version: 0.1
updated: 2026-10-17
depends_on:
  - ../../UI/ScreenTemplates.md
  - ../../UI/WidgetRegistry.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../UI/SemanticInput.md
  - ../../Status/ScreenRequests.md
  - ../../Proposals/CoreUiExtendedElementSetProposal.md
  - ../../Proposals/TemporaryGameModesProposal.md
decisions:
  - ../../ADR/0017-centralized-ui-presentation-paths.md
  - ../../ADR/0035-ui-foundation-and-composition.md
  - ../../ADR/0040-universal-ui-property-pipeline.md
  - ../../ADR/0046-test-content-coupling-boundary.md
---

# Экраны стартового пакета: план реализации

> **Материализует:** заявки [SR-001…SR-005](../../Status/ScreenRequests.md): неактивный пункт меню с подсказкой, отчёт о конце хода, магазин, диалог, сцена с участниками.
> **Задачи:** SPS-01…SPS-09 в файлах этапов ниже. Checkbox задачи — единственный источник её завершения.
> **Результат:** пять Screen Templates `textsystem` и два новых общих элемента (подсказка, пошаговый просмотр), проверенные production-путём показа и матрицей разрешений; потребители — задачи плана [RHStartPackage](../RHStartPackage/README.md).

## Граница плана

План — работа на стороне движка: schemas UI-полей, Widget Blueprint, Screen Registry, общие элементы `core`/`textsystem`, их contracts. Геймплей `rh` — отдельный план [RHStartPackage](../RHStartPackage/README.md); экраны не содержат правил игры и не знают `rh`.

Экран строится по [Add UI Screen](../../Authoring/AddUIScreen.md) и [Add Screen Field](../../Guides/AddScreenField.md): composite из существующих элементов на `UGV2DeclaredCompositeWidgetBase`, schema `ui_field`/`ui_value` данными, запись в `DA_ScreenRegistry`. C++ появляется только для UE-only возможности, которой нет у существующих элементов, и обосновывается по [When to Write Cpp](../../Guides/WhenToWriteCpp.md).

Editor assets (`.uasset`) создаются и меняются только через `unreal-mcp` с compile/save (`Tools/MCP/compile_and_save_assets.py`) и проверкой `Tools/MCP/run_ue_tests.py`. Если Editor API недоступен, задача останавливается.

## Что переиспользуется

| Потребность | Существующее |
|---|---|
| Кнопка, её стили `normal`/`hovered`/`pressed`/`disabled` | `WBP_Button`, CommonUI style в `DA_UITheme_Default` |
| Активность виджета из данных | Boolean consumer → `WidgetEnabled` |
| Повторяемые элементы по `key` | `CollectionHost`, `FGV2KeyedCollection` |
| Полоски | `WBP_ProgressBar`, форма `meters` из `location_player_status` |
| Длинный текст с прокруткой | `WBP_RichText`, `WBP_ScrollArea` |
| Выбор одного элемента с показом его содержимого (UE-local) | `UGV2TabContainerWidgetBase` |
| Всплывающее окно при наведении (UE-local, nested screen) | hover-окно `RichText` |
| Портрет, картинка, иконка | `WBP_Portrait`, `WBP_Image`, `WBP_Icon` |

Новых элементов два:

- **подсказка у нетекстового элемента** — строка «`Tooltip` за пределами текстовых фрагментов» из [Core UI Extended Element Set](../../Proposals/CoreUiExtendedElementSetProposal.md); первый потребитель — SR-001;
- **пошаговый просмотр коллекции** — одна запись за раз, «Далее» без вызова Lua; первый потребитель — SR-002.

## Зависимости вне плана

| Зависимость | Задачи | Почему |
|---|---|---|
| Editor API `unreal-mcp` и запущенный Unreal Editor | все, кроме SPS-01 | `.uasset` меняются только через Editor API |
| Механизм режимов: routing экрана активного режима ([Temporary Game Modes](../../Proposals/TemporaryGameModesProposal.md#13-этапы), M1–M2) | SPS-05…SPS-08, SPS-09 | экраны режимов показываются production-путём только через route активного режима |
| Источники меню [GAP-006](../../Status/EngineGaps.md#gap-006-сборка-меню-локации-из-источников-действий) | SPS-04 | без них ни один production-источник не публикует неактивный пункт |

[GAP-011](../../Status/EngineGaps.md#gap-011-локализуемые-аргументы-textspec) экраны не блокирует: имена передаются отдельными text-полями.

## Этапы и зависимости

| Этап | Задачи | Проверяемый результат |
|---|---|---|
| M0 — Общие элементы | SPS-01…SPS-03, [Elements](Elements.md) | Зафиксированные решения; элемент действия с неактивным состоянием и подсказкой; пошаговый просмотр |
| M1 — Экраны | SPS-04…SPS-08, [Screens](Screens.md) | Меню локации v2, отчёт о ходе, магазин, диалог, сцена с участниками |
| M2 — Приёмка | SPS-09, [Acceptance](Acceptance.md) | Production-путь каждого экрана, матрица разрешений, сверка contracts |

Зависимости: `01 → 02 → 04`; `01 → 03 → 05`; `02 → 06, 07, 08`; все → `09`.

- [ ] M0 — SPS-01…SPS-03 приняты по Done/Evidence.
- [ ] M1 — SPS-04…SPS-08 приняты по Done/Evidence.
- [ ] M2 — SPS-09 принята по Done/Evidence.

## Стыки с другими планами

| Задача | Потребитель | Что передаётся |
|---|---|---|
| SPS-04 | GAP-006, [RSP-06](../RHStartPackage/CoreLoop.md#rsp-06-меню-локации-с-доступностью) | `textsystem:schema.ui_field.location_commands.v2` с `enabled` и `disabled_reason` |
| SPS-05 | [RSP-10](../RHStartPackage/Modes.md#rsp-10-отчёт-о-конце-хода) | `textsystem:screen.turn_report` |
| SPS-06 | [RSP-11](../RHStartPackage/Modes.md#rsp-11-магазин) | `textsystem:screen.shop` |
| SPS-07 | [RSP-13](../RHStartPackage/Modes.md#rsp-13-диалог), RSP-14, RSP-15 | `textsystem:screen.dialogue` |
| SPS-08 | [RSP-16](../RHStartPackage/Modes.md#rsp-16-танец) | `textsystem:screen.participant_scene` |

## Проверка

- Каждая новая schema: положительный документ и отказы — неизвестный ключ, неверный тип или диапазон, дубликат и неверная грамматика `key`.
- Каждый новый composite автоматически попадает в `GV2.UI.CapabilityObservabilityCompositeSweep`.
- Каждый экран проверяется на шести разрешениях [матрицы](../../UI/ScreenTemplates.md#матрица-целевых-разрешений): обязательные контролы не обрезаются, длинный текст прокручивается внутри блока.
- Тесты экранов `textsystem` — content smoke ([ADR-0046](../../ADR/0046-test-content-coupling-boundary.md)): только в файлах из `smoke_files`, со структурными утверждениями и ожиданиями из контента. Механизмы новых общих элементов — contract-тесты на синтетических фикстурах `core:`.
- Перед коммитом: `validate_docs.py`, `ctest --test-dir cmake-build-ci`, `gv2-headless --check-scripts`, `Tools/MCP/run_ue_tests.py` для затронутых assets.

## Не входит в план

- Механизм режимов, источники меню (GAP-006) и панели статуса (GAP-007) — отдельные работы движка.
- Презентеры `rh`, строящие значения полей, — задачи RHStartPackage.
- Экран боя и другие экраны без заявки.
- Картинки и тексты контента: в шаблонах — только системные заглушки и подписи самих шаблонов.
- Остальные строки Core UI Extended Element Set: у них нет потребителя.
