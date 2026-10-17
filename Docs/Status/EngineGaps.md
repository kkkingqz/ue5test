---
title: Engine Gaps for Gameplay Authoring
status: informative
version: 1.3
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

## Стартовый пакет `rh`: что блокирует

Первый пакет реализации `rh` (2026-10-17) ждёт движок: временный хост режимов внутри `rh` не делается. Сводка зависимостей по пунктам пакета:

| Пункт | Что работает сейчас | Чего не хватает |
|---|---|---|
| 1. Десять локаций со связями | Definitions локаций и экранов, переходы, стоимость входа в расширении `rh` | Неактивные пункты меню — [SR-001](ScreenRequests.md#sr-001-неактивный-пункт-меню-с-подсказкой-о-причине); условные пункты меню — [GAP-006](#gap-006-сборка-меню-локации-из-источников-действий) |
| 2. Магазин: кнопка на локации открывает экран магазина, ассортимент, цены и описания из данных, покупка | Данные ассортимента (запас бесконечный, цены фиксированные), цена из Definition предмета, списание золота, создание предмета игроку `instances.create` | Экран режима в основном слое — [GAP-001](#gap-001-presentation-router-с-регистрацией-view-provider-из-пакета), [GAP-002](#gap-002-публикация-полного-ui-document-одним-действием), [GAP-003](#gap-003-сброс-временного-состояния-authoring-modules-между-lua-specs); экран магазина — [SR-003](ScreenRequests.md#sr-003-экран-магазина); имя предмета в тексте — [GAP-011](#gap-011-локализуемые-аргументы-textspec) |
| 3. Продажа: кнопка на локации, проверка заданных предметов игрока, оценка по формуле, текст в формате диалога | Формула оценки, условия, данные текстов | Поиск предметов игрока, свойства экземпляра предмета, удаление проданного — [GAP-004](#gap-004-операции-с-runtime-instance-не-actor-kind); вывод — режим диалога (пункт 4) |
| 4. Диалог: игрок и NPC на входе, ветвление по флагам, последовательность экранов «текст + картинка» с «Далее» или выбором ответа | Граф диалога в данных `rh`, словарь условий, флаги как отдельные объявленные поля | Режим и экран — GAP-001…GAP-003, [SR-004](ScreenRequests.md#sr-004-экран-диалога); флаги из данных — [GAP-008](#gap-008-поля-коллекции-и-целостность-ссылок-на-экземпляры); имя NPC в реплике — GAP-011 |
| 5. Событие перехода и реакции на него: условия, случайность, запуск взаимодействия со своими экранами; общая архитектура реакций | `emit`, `on`, отложенная команда (`:later`), `game.random` в команде; один активный режим разрешает конкуренцию реакций | Запуск взаимодействия — режимы (GAP-001…GAP-003); реакции, описанные данными, — [GAP-010](#gap-010-перебор-definitions-по-kind-в-authoring); порядок реакций, объявленных в Lua, — [GAP-013](#gap-013-приоритет-подписчика-в-authoring-on) |
| 6. Танец: два и больше NPC, временные характеристики сцены, кнопки действий, результат участникам | Передача списка персонажей в аргументах команды, расчёт в Lua, запись результата | Режим и экран — GAP-001…GAP-003, [SR-005](ScreenRequests.md#sr-005-экран-танца); базовые характеристики из Definition — [GAP-012](#gap-012-поля-definition-из-блока-расширения-пакета); случайность внутри режима — [Temporary Game Modes § 9](../Proposals/TemporaryGameModesProposal.md#9-rng-в-режимах) |

Реакция на событие не выполняется в самом подписчике: по [Commands and Events](../Architecture/CommandsAndEvents.md) подписчик работает при закрытом mutation window, а `game.random` пишет состояние потока в canonical `meta.prng`. Поэтому подписчик ставит в очередь команду реакции, а условия, бросок и запуск режима выполняет она. Это следствие contract, а не пробел.

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
  - режим может запускаться отложенной командой (реакция на событие перехода): нужно проверить и при необходимости обеспечить, что UI не получает интерактивную промежуточную revision экрана локации между командой перехода и командой запуска режима.
- **Потребители стартового пакета:** магазин, продажа (вывод оценки), диалог, реакции на события, танец, отчёт о конце хода.
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
- **Требуется:** штатный способ объявить временное состояние authoring module так, чтобы spec runner сбрасывал его между case-ами, а также, при необходимости, отладочное чтение этого состояния из spec. Например, объявленная authoring-секция transient state с явной функцией reset вместо голых upvalue. Вместе с этим — восстановление временного состояния режима при `fail()` или ошибке команды ([Temporary Game Modes § 4](../Proposals/TemporaryGameModesProposal.md#4-модель-состояния)).
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

- **Потребность:** инвентарь, хранилище дома, процессы, эффекты и лут — экземпляры с владельцем, которые нужно найти, перечислить, передать и удалить. В стартовом пакете — продажа: найти у игрока предметы заданных Definitions, прочитать свойства конкретного экземпляра (например, состояние или качество предмета) для формулы оценки, удалить проданный экземпляр.
- **Сейчас:** authoring `instances` даёт только `register_kind` и `create` (`Scripts/authoring/context.lua`); `Scripts/runtime/instance_registry.lua` не имеет get/list/remove. Поэтому `GameData/rh/scripts/gameplay/actors.lua` читает и пишет `game.state.item_instances` напрямую. Поле `field.ref_instance(kind)` при чтении разрешается только через `game.instances.actors.get` (`Scripts/runtime/actor_registry.lua`, `Scripts/authoring/properties.lua`), поэтому ссылка на предмет или процесс возвращает голый ID.
- **Требуется:**
  - `instances.get(id)` → wrapper экземпляра любого kind;
  - перечисление экземпляров по kind, по владельцу и по Definition в детерминированном порядке (например, `instances.owned_by(player, "item")`);
  - смена владельца (персонаж, локация как хранилище) и удаление экземпляра с проверкой ссылок на него;
  - поля экземпляра через prototype (`Item.condition = field.integer({ min = 0, max = 100 })`) и методы prototype (`function Item:appraise()`) на wrapper экземпляра;
  - доступ из wrapper экземпляра к его Definition (`item.definition.price`);
  - разрешение `ref_instance` для любого kind.
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

### GAP-008. Поля-коллекции и целостность ссылок на экземпляры

- **Потребность:** роли-множества (`player.companions`), списки участников игромеханических моделей конца хода, ограничение кандидата на роль (например, только персонаж с тегом `dog`). Флаги игрока и NPC, по которым ветвятся диалоги и реакции: имена флагов задаются в данных диалога, поэтому объявлять каждый флаг отдельным полем в Lua противоречит принципу «сначала данные».
- **Сейчас:** `Scripts/authoring/field.lua` даёт только скалярные constructors, `enum`, `ref_definition` и `ref_instance`; constructor списка нет, хотя `Scripts/authoring/properties.lua` уже обрабатывает kind `array`. Проверка висячих ссылок на экземпляры в `Scripts/runtime/state_validator.lua` охватывает только `owner_id` предметов и `meta.player_actor_id`; поле-роль на удалённого персонажа не проверяется. Ограничения на кандидата у `ref_instance` нет.
- **Требуется:**
  - `field.list(item_descriptor, opts)`;
  - поле-словарь с типизированными ключами и значениями, например флаги, где ключ — Stable ID Definition kind `flag` (`rh:flag.met_merchant`), а значение — boolean или integer; ключ проверяется по repository, поэтому опечатка в данных диалога ловится при сборке и чтении, а не молча создаёт новый флаг;
  - проверка или явная политика (запрет удаления либо обнуление) для `ref_instance`-полей при удалении экземпляра;
  - необязательное ограничение кандидата `ref_instance` (по тегу или Definition).
- **Замечание:** сегодня запись необъявленного поля в персонажа проходит (`Scripts/runtime/actor_registry.lua`, ветка без schema field пишет значение как есть), хотя [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md#справочник-field) называет unknown field отклоняемым. Геймплей на это поведение не опирается.
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

- **Потребность:** фазы конца хода `rh` — отдельные Definitions (`rh:turn_phase.<name>` с полем `order`); конец хода исполняет все фазы по возрастанию `order`, а мод добавляет фазу новой Definition без переопределения общего списка. Тот же перебор нужен для других расширяемых наборов: реакций на события (Definition реакции: событие, условия, шанс, приоритет, запускаемое взаимодействие), квестов, обходов контента в specs.
- **Сейчас:** `def.<kind>(name)` возвращает одну Definition по имени (`Scripts/authoring/context.lua`). Перебор есть только в programmer API `game.repository.list` ([GameDataRepository Contract](../Architecture/GameDataRepositoryContract.md)); в authoring `_ENV` его нет.
- **Требуется:** authoring-перебор Definitions указанного kind (возможно, с фильтром по тегу) в каноническом детерминированном порядке, возвращающий definition wrappers.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md).
- **Приоритет:** блокирует конец хода в выбранной форме.
- **Статус:** open.

### GAP-011. Локализуемые аргументы TextSpec

- **Потребность:** подставлять в текст локализуемые имена: имя NPC в реплике диалога, название предмета в оценке при продаже и в магазине, имя собаки в отчёте о конце хода («{dog} принесла зайца»).
- **Сейчас:** аргументы `TextSpec` обязаны быть скалярами: UE text pipeline отклоняет иные значения (`Source/GV2/Private/UI/GV2TextPipeline.cpp`, «Text arguments must be scalar non-null values»), `Scripts/resources/text.lua` проверяет только, что `args` — таблица. Lua не разрешает локализацию ([ADR-0013](../ADR/0013-unified-text-pipeline.md): headless хранит unresolved `TextSpec`), поэтому подставить уже переведённое имя строкой нельзя.
- **Требуется:** типизированный аргумент-ссылка на текст (вложенный `TextSpec` или `text_id`), который разрешается тем же pipeline при показе; одинаковое представление в headless и UE.
- **Затрагивает:** [ADR-0013](../ADR/0013-unified-text-pipeline.md), [UI Document](../UI/UIDocumentAndReconciliation.md), [Presentation Authoring Reference](../Authoring/PresentationAuthoringReference.md#создать-текст).
- **Статус:** open.

### GAP-012. Поля Definition из блока расширения пакета

- **Потребность:** базовые характеристики персонажа (для формул, танца, оценки) задаются в Definition персонажа и читаются как типизированные поля: `Actor.strength = field.integer({ storage = "definition" })`.
- **Сейчас:** схема `actor` принадлежит `core` и содержит в `data` только `discriminator` (`GameData/core/schemas/actor_v1.schema.json5`); игровые поля `rh` живут в блоке `extensions.rh` (`GameData/rh/schemas/actor_rh_extension_v1.schema.json5`). Поле со `storage = "definition"` читается только из `data` (`Scripts/runtime/actor_registry.lua`, ветка `fspec.storage == "definition"`), поэтому до значений расширения через `field.*` не дотянуться — только сырой таблицей `extensions` Definition, без контракта и default.
- **Требуется:** field descriptor со `storage = "definition"`, читающий значение из блока расширения объявляющего пакета (или явно указанного), с проверкой согласованности с extension schema при freeze.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Definition Envelope and Schema Rules](../Architecture/DefinitionEnvelopeAndSchemaRules.md), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md#справочник-field).
- **Статус:** open.

### GAP-013. Приоритет подписчика в authoring `on()`

- **Потребность:** несколько реакций на одно событие (например, на переход) объявлены в Lua; порядок их проверки задаёт игра, а не порядок загрузки файлов.
- **Сейчас:** [Commands and Events](../Architecture/CommandsAndEvents.md) упорядочивает подписчиков по priority, порядку пакета и порядку регистрации, но authoring `on(event, fn)` не принимает priority (`Scripts/authoring/context.lua`, `mod.on`).
- **Требуется:** необязательный priority в `on(event, fn, opts)`.
- **Приоритет:** низкий: при реакциях, описанных данными (GAP-010), порядок задаёт `rh`.
- **Статус:** open.

## Авторский слой пакета с нуля

Источник: решение от 2026-10-17 переписать `rh` с нуля, удалив текущую заглушку. Пробелы ниже не относятся к конкретной механике: они мешают пакету игры целиком жить на authoring surface и проверяться своими specs.

### GAP-014. Поля и методы prototype на Definitions

- **Потребность:** стадия и флаги квеста — runtime-state поля Definition квеста ([RH Gameplay Architecture § 5.11](../RH/GameplayArchitecture.md#511-квесты)); состояние локации — runtime-state поля Definition локации; методы на Definitions собственных kinds `rh` (`function Shop:assortment()`, методы фазы конца хода `rh:turn_phase.*`).
- **Сейчас:** prototype `Quest`, `Location` или пользовательский PascalCase регистрирует field descriptors под своим именем (`"Quest"`, `Scripts/authoring/context.lua`, `create_entity_prototype_proxy`). Wrapper Definition ищет schema по discriminator или по kind в нижнем регистре (`"quest"`, `Scripts/authoring/properties.lua`, `wrap_definition`), поэтому объявленное поле не находится. Проверено пробным spec: после `Quest.stage = field.non_negative_integer()` чтение `stage` у Definition `rh:quest.main` возвращает `nil`, запись даёт `Cannot modify definition field 'stage' (no runtime_state schema defined)`; так же ведёт себя пользовательский prototype для kind `turn_phase`. Wrapper экземпляра персонажа ищет schema через `get_effective_schema(..., "Actor")`, поэтому у `Actor` поля работают. Методы Definition ищутся под именем kind с заглавной первой буквой (`Turn_phase` для `turn_phase`), а prototype `TurnPhase` регистрируется под `"TurnPhase"`: соответствие имени prototype и многословного kind не определено (по коду, без пробного запуска).
- **Требуется:**
  - field descriptors и методы prototype применяются к Definitions соответствующего kind через `def.<kind>(name)` и `location(name)`;
  - определённое соответствие имени prototype и kind (`TurnPhase` ↔ `turn_phase`);
  - conformance через production-путь wrapper Definition, а не прямой вызов `register_schema("quest", ...)`, как в нынешних specs.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md#entities-fields-and-references), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md#объявить-поля-и-методы-сущности).
- **Приоритет:** блокирует квесты в принятой форме и методы на собственных kinds `rh`.
- **Статус:** open.

### GAP-015. Старт новой игры из authoring

- **Потребность:** начальное состояние новой игры (игрок, NPC, их размещение, стартовые предметы, роли и флаги) создаётся командой пакета по данным сценария старта, без кода вне authoring surface.
- **Сейчас:** [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md) даёт пакету `start` hook module, который создаёт начальное состояние через зарегистрированную command. У authoring module возвращаемое значение игнорируется, поэтому объявить `start` в нём нельзя. В `rh` это делает обычный module `GameData/rh/scripts/runtime/session_start.lua` через programmer API: определяет новую игру по `game.instances.actors.player() == nil` и вызывает `game.runtime.dispatch_command`.
- **Требуется:** authoring-объявление стартовой команды пакета (например, `new_game = commands.start` или поле в manifest пакета), которую lifecycle вызывает только при новой игре, а не после load; признак «новая игра» определяет lifecycle, а не проверка наличия игрока.
- **Временно:** `rh` сохраняет один module-мост вне authoring: вызов стартовой команды без логики.
- **Затрагивает:** [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md).
- **Статус:** open.

### GAP-016. Место specs пакета игры

- **Потребность:** specs механик, обходы контента и эталонные прогоны `rh` ([RH Gameplay Architecture § 9](../RH/GameplayArchitecture.md#9-проверка)) лежат рядом с пакетом и исполняются на реальном пакете `rh` и его зависимостях.
- **Сейчас:** spec runner находит specs только в `Tests/Lua/<subtree>/`, а соответствие subtree уровню пакетов задано списком в C++ (`Source/GV2TestSupport/Private/LuaSpecRunner.cpp`, `GetAllSubtreeTierConfigs`); незарегистрированный subtree — ошибка. Subtrees уровня FullGame (`authoring`, `economy`, `presentation`) заняты specs движка. В каталоге пакета места для specs нет.
- **Требуется:** каталог specs пакета (например, `GameData/<pkg>/tests/`), который runner находит сам и исполняет на сессии из пакета и его зависимостей; классификация таких specs по [ADR-0046](../ADR/0046-test-content-coupling-boundary.md) как тестов игры, а не contract-тестов движка.
- **Затрагивает:** [Add Lua Spec](../Guides/AddLuaSpec.md), [Build and Tooling](../Architecture/BuildAndTooling.md#test-content-decoupling), [Headless Simulation](../Architecture/HeadlessSimulationContract.md).
- **Приоритет:** блокирует стратегию проверки `rh`.
- **Статус:** open.

### GAP-017. Тесты движка опираются на заглушку `rh`

- **Потребность:** удалить текущую заглушку `rh` (команды `travel`, `time.wait_day`, `work.do_work`, `request_buy`/`buy`, их Definitions, экраны и тексты) и написать пакет заново.
- **Сейчас:** проверки движка используют содержимое заглушки:
  - Lua specs уровня FullGame на реальном пакете `rh`: `Tests/Lua/economy/` (например, `travel_stamina.lua` вызывает `rh:command.travel`), `Tests/Lua/presentation/` (например, `dynamic_menu.lua` ждёт binding `rh:command.request_buy`), часть `Tests/Lua/authoring/`;
  - C++ тесты, загружающие пакет `rh` или его расширения (например, `GV2ContentSmokeTests.cpp` читает блок `extensions.rh`);
  - Editor profile `core + textsystem + rh` (`Config/DefaultGame.ini`, `EditorPackageRoots`), где `rh` обязан создать начальное состояние и показать `textsystem:screen.location` ([Build and Tooling](../Architecture/BuildAndTooling.md)).
- **Требуется:** до удаления заглушки перевести эти проверки на синтетические фикстуры или fixture package движка ([ADR-0046](../ADR/0046-test-content-coupling-boundary.md), `STATUS-028` в [Implementation Status](ImplementationStatus.md)) либо явно перечислить, что из содержимого `rh` остаётся контрактом для тестов движка.
- **Приоритет:** блокирует удаление заглушки; новый `rh` нельзя положить на место старого, пока на старый опираются тесты.
- **Статус:** open.

### GAP-018. Имя экземпляра и удаление персонажа из authoring

- **Потребность:** типовые NPC генерируются из шаблона — на старте по сценарию и во время игры (например, встречный бандит), — а код обращается к конкретному экземпляру: квест или контентный скрипт ссылается на сгенерированную собаку «Рекс» ([RH Gameplay Architecture § 5.9](../RH/GameplayArchitecture.md#59-персонажи-уникальные-и-типовые)). Роль связывает экземпляр с правилом динамически, но контенту нужна и постоянная ссылка, известная заранее. Ненужный сгенерированный NPC (убитый бандит) удаляется.
- **Сейчас:** instance ID (`actor@7`) выдаёт allocator при создании ([Stable ID Specification](../Architecture/StableIDSpecification.md#persistent-instance-ids)), поэтому заранее он неизвестен. `actor(name)` ищет по Definition и требует ровно один экземпляр (`ActorInstanceAmbiguous`), так что среди экземпляров одного шаблона конкретный не выбрать. Authoring `instances` даёт только `register_kind` и `create` (`Scripts/authoring/context.lua`); удаление персонажа с отказом при ссылках на него есть только в runtime registry (`Scripts/runtime/actor_registry.lua`, `remove`, `ActorHasDependentReferences`).
- **Требуется:**
  - постоянное имя экземпляра, задаваемое при создании (данными сценария или кодом): уникально в сессии, сохраняется в save, дубликат отклоняется при создании, имя освобождается при удалении; namespace — пакет, создавший экземпляр;
  - поиск экземпляра по имени из authoring с typed отказом при отсутствии;
  - удаление персонажа из authoring с той же политикой ссылок, что в GAP-008.
- **Затрагивает:** [Stable ID Specification](../Architecture/StableIDSpecification.md#persistent-instance-ids), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md#entities-fields-and-references), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md).
- **Приоритет:** блокирует обращение к сгенерированным NPC из контента; генерация и роли работают без него.
- **Статус:** open.
