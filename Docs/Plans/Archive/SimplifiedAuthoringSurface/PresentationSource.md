---
title: Presentation Source Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - README.md
  - ../../../UI/ScreenTemplates.md
---

# M4 — Presentation Source

> **Материализует:** [Screen Templates](../../../UI/ScreenTemplates.md) в части того, кто перестраивает экран.
> **Задачи:** SAS-14…16.
> **Результат:** геймплей не знает про интерфейс; экран обновляется сам после успешной команды.

## Результат этапа

Вызовов вида `build_and_publish_screen()` в командах не остаётся.

Целевая семантика — «активный маршрут заново разрешает желаемое состояние экрана» — закладывается **архитектурно**: маршрутизации не существует (`UI document: routes, layers, overlays` не реализован, активен один экран за раз). Поэтому вводится шов, который маршрутизатор позже займёт собой, не затрагивая ни строки геймплея.

## Задачи

- [x] **SAS-14 — Источник презентации**
  - Пакет регистрирует функцию, разрешающую желаемый экран из текущего состояния.
  - Done: регистрация выполняется на фазе `register` (`game.presentation.register_source(fn)`) и замораживается вместе с остальными реестрами; источник — функция без аргументов, читающая состояние; повторная регистрация (`PresentationSourceDuplicateRegistration`), невалидный тип (`InvalidPresentationSource`) и регистрация после freeze (`PresentationSourceRegistryFrozen`) дают раздельные отказы; отсутствие источника не является ошибкой.
  - Evidence: `Scripts/runtime/presentation_source.lua`, `Scripts/bootstrap/manifest.lua`, `Scripts/bootstrap/main.lua`, `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Tests/Lua/authoring/simplified_surface.lua` (`presentation_source_registration_and_validation`).

- [x] **SAS-15 — Автоматическая инвалидация после commit**
  - Зависимости: SAS-14.
  - Done: рантайм вызывает источник через `game.presentation.resolve()` после каждой **успешно закоммиченной** команды (`command_dispatcher.lua`); при отказе и при fault источник не вызывается; вызов происходит вне окна мутации, поэтому попытка мутации состояния даёт `MutationWindowClosed` / `StateWriteOutsideMutationWindow`; повторное разрешение после каждой команды принято как поведение v1; спеки проверяют обновление экрана после успешной команды и отсутствие обновления при отказе.
  - Evidence: `Scripts/runtime/command_dispatcher.lua`, `GameData/rh/scripts/presentation/location_screen.lua`, `Tests/Lua/authoring/simplified_surface.lua` (`automatic_invalidation_after_successful_command`, `presentation_source_state_mutation_disallowed`), `Tests/Lua/presentation/dynamic_menu.lua`.

- [x] **SAS-16 — Запретить обновление презентации из геймплея и синхронизировать contract**
  - Зависимости: SAS-15.
  - Done: designer-facing геймплей-команды (`shop.lua`, `work.lua`, `time.lua`, `travel.lua`) очищены от ручных вызовов `build_and_publish_screen()` и зависимостей на `location_screen`; [Screen Templates](../../../UI/ScreenTemplates.md) и [Commands and Events](../../../Architecture/CommandsAndEvents.md) описывают источник презентации и автоматическую инвалидацию; в [Implementation Status](../../../Status/ImplementationStatus.md) зафиксирован шов под будущий UI document.
  - Evidence: `GameData/rh/scripts/gameplay/` (`shop.lua`, `time.lua`, `work.lua`, `travel.lua`), `Docs/UI/ScreenTemplates.md`, `Docs/Architecture/CommandsAndEvents.md`, `Docs/Status/ImplementationStatus.md`.

## Проверка milestone

- [x] Экран перестраивается после успешной команды без участия геймплея.
- [x] После отклонённой команды экран не перестраивается.
- [x] Источник не может изменить состояние.
- [x] Designer-facing API не содержит способа обновить экран вручную.
