---
title: Engine Gaps for Gameplay Authoring
status: informative
version: 1.0
updated: 2026-10-17
depends_on:
  - ../Architecture/AuthoringSurfaceContract.md
  - ../Architecture/RuntimeFacadeAndRegistries.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../Guides/AddLuaSpec.md
---

# Пробелы движка для геймплея на authoring Lua

> **Показывает:** возможности, которых не хватает authoring surface для геймплея пакетов; документ фиксирует потребность и не вводит новых правил.

Геймплей пакетов пишется только на authoring surface (`<package>/scripts/authoring/`). Если нужной возможности нет, геймплей не обходит её через `game.*`, сырое `game.state` или собственный presentation module, а пробел записывается сюда и реализуется отдельно, на стороне движка.

Формат записи: потребность геймплея, текущее состояние, что требуется, затронутые contracts, статус.

## Временные игровые режимы

Источник: обсуждение архитектуры временных режимов (бой, торговля, диалог) от 2026-10-17. Принятое направление:

- Location — режим по умолчанию; одновременно активен не более одного временного режима.
- Состояние режима (Mode Run) не сохраняется: после load или restart открывается Location по `location_id` игрока.
- Команды режима — обычные `commands.*` с валидаторами, отказами и action bindings; отдельного диспетчера `handle(state, command, args)` нет. Общий контракт режима: `start`, `build_view`, `finish`, `cancel`.
- Политика записи в canonical state задаётся каждым режимом: торговля применяет результат одной командой в конце; диалог применяет последствия выбора сразу обычными командами, временным остаётся только позиция в дереве; бой — результат в конце, RNG по отдельному решению.
- Команда режима соблюдает «сначала все проверки, потом изменения» и для временного состояния: `AuthoringFailAfterMutation` отслеживает только canonical `write_revision`, поэтому `fail()` после изменения временной таблицы её не откатит.
- Сохранение во время режима не запрещается правилом: canonical state согласован, в save просто не попадает незавершённый режим. UI режима не показывает кнопку сохранения.

Сами режимы реализуются на authoring surface. Для их показа и тестирования не хватает трёх возможностей ниже. Решение предложено в [Temporary Game Modes](../Proposals/TemporaryGameModesProposal.md).

### GAP-001. Presentation router с регистрацией view provider из пакета

- **Потребность:** пока режим активен, вместо экрана локации показывать экран режима, построенный пакетом-владельцем (`rh`).
- **Сейчас:** `game.presentation.register_source` допускает один source (`PresentationSourceDuplicateRegistration`, `Scripts/runtime/presentation_source.lua`). Его занимает `textsystem:module.presentation.location_presenter` — обычный, не authoring module. В authoring `_ENV` нет способа объявить построитель экрана: доступны только разовые `show_*` внутри команд, а после каждой успешной команды `command_dispatcher` вызывает `game.presentation.resolve()`, который перестраивает экран через location presenter.
- **Требуется:**
  - единственный presentation source — маршрутизатор: если активного режима нет, строится Location, иначе view активного режима;
  - authoring-объявление view provider режима, например `modes.trade = { view = function() ... end }` или аналог, без прямой зависимости `textsystem` → `rh`; цепочка `core → textsystem → rh` сохраняется;
  - location presenter становится переиспользуемым builder без собственной регистрации; ветки тестовых фикстур `dca09`/`dca10`/`dca11`/`duc10` сохраняют совместимый путь.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Runtime Facade](../Architecture/RuntimeFacadeAndRegistries.md), [UI Document](../UI/UIDocumentAndReconciliation.md), `GameData/textsystem/scripts/presentation/location_presenter.lua`.
- **Статус:** open; решение — [Temporary Game Modes](../Proposals/TemporaryGameModesProposal.md).

### GAP-002. Публикация полного UI document одним действием

- **Потребность:** при смене режима (Location → Trade → Location) не оставлять overlays и modals предыдущего режима.
- **Сейчас:** `show_screen`/`show_route` заменяют только `route` (`Scripts/authoring/presentation.lua`, `active_document_state.route = inst`), а `overlays` и `modals` остаются. Очистить все слои можно только поштучными `close_overlay`/`close_modal`, причём автор должен знать ключи, открытые другим режимом. Каждый вызов публикует отдельную revision, то есть возникают промежуточные интерактивные документы.
- **Требуется:** authoring helper полной публикации документа (условно `replace_document({ route = ..., overlays = ..., modals = ... })`) или scoped cleanup слоёв, принадлежащих режиму, в одной revision.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [UI Document](../UI/UIDocumentAndReconciliation.md), [Presentation Authoring Reference](../Authoring/PresentationAuthoringReference.md).
- **Статус:** open; решение — [Temporary Game Modes](../Proposals/TemporaryGameModesProposal.md).

### GAP-003. Сброс временного состояния authoring modules между Lua specs

- **Потребность:** изолированно тестировать режимы, чьё состояние живёт в `local`-таблицах authoring module, а не в canonical state.
- **Сейчас:** `core:module.runtime.test_isolation` и `snapshot_test_state`/`restore_test_state` изолируют registries (services, handlers, validators, actors) и canonical state. Локальные таблицы модулей они не видят, поэтому активный режим из одного case попадает в следующий, а state hash его не учитывает.
- **Требуется:** штатный способ объявить временное состояние authoring module так, чтобы spec runner сбрасывал его между case-ами, а также, при необходимости, отладочное чтение этого состояния из spec. Например, объявленная authoring-секция transient state с явной функцией reset вместо голых upvalue.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Add Lua Spec](../Guides/AddLuaSpec.md), [Headless Simulation](../Architecture/HeadlessSimulationContract.md).
- **Статус:** open; решение — [Temporary Game Modes](../Proposals/TemporaryGameModesProposal.md).
