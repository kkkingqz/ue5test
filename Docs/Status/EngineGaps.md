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
- Команды режима — обычные `commands.*` с валидаторами, отказами и action bindings; отдельного диспетчера `handle(state, command, args)` нет. Общий контракт режима: `start`, `view`, `finish`, `cancel`.
- Политика записи в canonical state задаётся каждым режимом: торговля применяет результат одной командой в конце; диалог применяет последствия выбора сразу обычными командами, временным остаётся только позиция в дереве; бой получает на вход значения участников, не сдвигает время мира и возвращает изменения в конце; RNG по отдельному решению.
- Состояние режима транзакционно вместе с командой: при `fail()` или ошибке оно восстанавливается. Сегодня `AuthoringFailAfterMutation` отслеживает только canonical `write_revision`, и `fail()` после изменения `local`-таблицы её не откатывает.
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

## Базовый игровой цикл

Источник: обсуждение принципов игры от 2026-10-17. Геймплей пишется в пакете `rh`, общие механизмы относятся к `textsystem` и `core`. Принятые принципы:

- Правила работают с любым персонажем: набор характеристик у игрока и NPC одинаковый, правило получает персонажа параметром.
- Своих ходов у NPC нет. NPC действует только в ответ на действие игрока или в конце хода по заданному условию (например, флаг NPC → действие с его характеристиками и очками).
- Ходы NPC в конце хода выполняют конкретные игромеханические модели, которые обращаются к конкретным NPC через роли и списки участников, а не перебор всех NPC мира.
- Действие — основная единица геймплея игрока. Число действий ограничено выносливостью; когда она исчерпана, остаётся только завершение хода. Длительность хода — данные пакета (в `rh` — 6 часов), восстановление выносливости — формула пакета.
- Процесс (например, алхимическое зелье) — объект с прогрессом, на который тратят ресурсы (выносливость, материалы) игрок и NPC по его команде. Результат выдаётся сразу, когда вложено достаточно; конец хода процесс не продвигает.
- Меню локации собирается из источников: действия игрока, действия спутников, действия самой локации и находящихся в ней объектов, переходы. Для конкретной локации стандартная сборка может быть переназначена.
- Недоступное действие показывается неактивным с подсказкой о причине (заявка SR-001 в [Screen Requests](ScreenRequests.md)).
- Переход стоит выносливости; стоимость задаётся в Definition локации.
- Потеря сознания (у игрока и NPC) блокирует любые действия, кроме завершения хода.
- Каждая фаза конца хода возвращает результат «картинка + описание» или пустой результат; непустые результаты показываются отдельным экраном в основном слое через механизм режимов (заявка SR-002). Фаза конца хода — отдельная Definition с номером порядка; фазы исполняются по возрастанию номера.
- В игре есть квесты, сюжет и песочница. Масштаб целевой игры — сотни NPC. Расписания NPC пока не делаются.
- Роль — поле-ссылка на конкретный экземпляр (`player.dog`); общие обработчики работают с ролью, а не с конкретным ID.
- Бой, торговля и диалог — временные режимы (выше); дом — локация с владельцем и объектами взаимодействия, а не режим.

### GAP-004. Операции с Runtime Instance не-actor kind

- **Потребность:** инвентарь, хранилище дома, процессы, эффекты и лут — экземпляры с владельцем, которые нужно найти, перечислить, передать и удалить.
- **Сейчас:** authoring `instances` даёт только `register_kind` и `create` (`Scripts/authoring/context.lua`); `Scripts/runtime/instance_registry.lua` не имеет get/list/remove. Поэтому `GameData/rh/scripts/gameplay/actors.lua` читает и пишет `game.state.item_instances` напрямую. Поле `field.ref_instance(kind)` при чтении разрешается только через `game.instances.actors.get` (`Scripts/runtime/actor_registry.lua`, `Scripts/authoring/properties.lua`), поэтому ссылка на предмет или процесс возвращает голый ID.
- **Требуется:** authoring-операции `instances.get(id)`, перечисление по kind и по владельцу в детерминированном порядке, смена владельца, удаление с проверкой ссылок; wrapper экземпляра любого kind с методами его prototype (`Item`, custom PascalCase); разрешение `ref_instance` для любого kind.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md).
- **Приоритет:** блокирует инвентарь, торговлю, дом и процессы.
- **Статус:** open.

### GAP-005. Запросы к персонажам по локации

- **Потребность:** меню локации и правила реакции должны знать, какие NPC находятся в текущей локации.
- **Сейчас:** `actor(name)` и `actors(name)` отбирают только по actor Definition и перебирают все `game.instances.actors.ids()` (`Scripts/authoring/context.lua`). Отбора по локации, тегу или владельцу нет.
- **Требуется:** authoring-запрос персонажей в локации (и по тегу) с детерминированным порядком. При целевом масштабе в сотни NPC перебор допустим; индекс — оптимизация, а не условие.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md).
- **Статус:** open.

### GAP-006. Сборка меню локации из источников действий

- **Потребность:** стандартное меню локации из четырёх источников (игрок, спутники, локация и её объекты, переходы) с неактивными пунктами и причиной недоступности; переопределение сборки для конкретной локации.
- **Сейчас:** меню строит `textsystem:module.presentation.location_presenter` — обычный, не authoring module. Он берёт статичный список `actions` из screen Definition локации и добавляет переходы из `connected_location_ids`. Объявить источник действий, условие доступности или переопределение для локации из authoring module нельзя.
- **Требуется:** в `textsystem` — authoring-объявление источников действий меню (источник возвращает пункты: `key`, текст, binding, доступность, причина недоступности, порядок/группа), стандартный порядок источников и переопределение сборки для конкретной локации; пакет `rh` объявляет свои источники, `textsystem` не зависит от `rh`.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Screen Templates](../UI/ScreenTemplates.md), [Presentation Authoring Reference](../Authoring/PresentationAuthoringReference.md), `GameData/textsystem/scripts/presentation/location_presenter.lua`.
- **Приоритет:** блокирует основной игровой цикл.
- **Статус:** open.

### GAP-007. Данные панелей статуса из пакета игры

- **Потребность:** панель статуса и верхняя панель показывают характеристики, выбранные игрой: полоски HP и выносливости, эффекты, время суток.
- **Сейчас:** `location_presenter` жёстко берёт золото и выносливость (`get_gold`, `get_stamina`), одну полоску `stamina` с делением на 100, предметы из `game.state.item_instances`, `effects = {}` и день из `game.state.meta.day`.
- **Требуется:** authoring-способ для пакета игры задать значения `top_bar` и `player_status` (meters, items, effects, основной ресурс, время); `textsystem` не знает имён характеристик `rh`.
- **Затрагивает:** [Screen Templates](../UI/ScreenTemplates.md), [Presentation Authoring Reference](../Authoring/PresentationAuthoringReference.md), `GameData/textsystem/scripts/presentation/location_presenter.lua`.
- **Статус:** open.

### GAP-008. Поля-списки и целостность ссылок на экземпляры

- **Потребность:** роли-множества (`player.companions`), списки участников игромеханических моделей конца хода, ограничение кандидата на роль (например, только персонаж с тегом `dog`).
- **Сейчас:** `Scripts/authoring/field.lua` даёт только скалярные constructors, `enum`, `ref_definition` и `ref_instance`; constructor списка нет, хотя `Scripts/authoring/properties.lua` уже обрабатывает kind `array`. Проверка висячих ссылок на экземпляры в `Scripts/runtime/state_validator.lua` охватывает только `owner_id` предметов и `meta.player_actor_id`; поле-роль на удалённого персонажа не проверяется. Ограничения на кандидата у `ref_instance` нет.
- **Требуется:** `field.list(item_descriptor, opts)` для authoring; проверка или явная политика (запрет удаления либо обнуление) для `ref_instance`-полей при удалении экземпляра; необязательное ограничение кандидата `ref_instance` (по тегу или Definition).
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md).
- **Статус:** open.

### GAP-009. Field contracts для world

- **Потребность:** время мира, счётчик ходов и глобальные флаги `rh` с типом и ограничениями.
- **Сейчас:** `world` пишет в `state.world` без схемы (`Scripts/runtime/world.lua`); `field.*` применяется только к prototype сущностей.
- **Требуется:** объявление полей `world` через `field.*` (например, `World.turn = field.non_negative_integer()`) с тем же guard, что у сущностей.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md).
- **Приоритет:** низкий: запись в `world` без контракта работает.
- **Статус:** open.

### GAP-010. Перебор Definitions по kind в authoring

- **Потребность:** фазы конца хода `rh` — отдельные Definitions (`rh:turn_phase.<name>` с полем `order`); конец хода исполняет все фазы по возрастанию `order`, а мод добавляет фазу новой Definition без переопределения общего списка. Тот же перебор нужен для других расширяемых наборов (например, квестов).
- **Сейчас:** `def.<kind>(name)` возвращает одну Definition по имени (`Scripts/authoring/context.lua`). Перебор есть только в programmer API `game.repository.list` ([GameDataRepository Contract](../Architecture/GameDataRepositoryContract.md)); в authoring `_ENV` его нет.
- **Требуется:** authoring-перебор Definitions указанного kind (возможно, с фильтром по тегу) в каноническом детерминированном порядке, возвращающий definition wrappers.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md).
- **Приоритет:** блокирует конец хода в выбранной форме.
- **Статус:** open.
