---
title: Engine Gaps for Gameplay Authoring
status: informative
version: 1.3
updated: 2026-10-18
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

Порядок закрытия и критерий закрытия каждой записи — [Engine Gap Closure](../Plans/EngineGapClosure/README.md). Этапы выполняются последовательно: движок закрывается полностью до начала работы над экранами, экраны — до начала работы над контентом.

## Игра «Охотник»: что блокирует

Первая игра `rh` — «Охотник» ([Hunter Gameplay](../RH/HunterGameplay.md)), устроенная по [архитектуре геймплея RH](../RH/GameplayArchitecture.md); 2026-10-18 она заменила стартовый пакет из шести пунктов. Игра ждёт движок: временные обходы внутри `rh` не делаются. Сводка зависимостей:

| Возможность игры | Чего не хватает |
|---|---|
| Сцены диалога (охота, вступление, город, конец игры), бой с совомедведем, магазины, танец | Маршрут экрана режима и публикация документа — [GAP-001](#gap-001-presentation-router-с-регистрацией-view-provider-из-пакета), [GAP-002](#gap-002-публикация-полного-ui-document-одним-действием); шаблоны экранов готовятся отдельно |
| Общий интерфейс игрока на всех экранах, выбор режима — показывать ли его | [GAP-020](#gap-020-общий-интерфейс-при-смене-сменной-части-экрана), значения панелей — [GAP-007](#gap-007-данные-панелей-статуса-из-пакета-игры) |
| Смерть от усталости и победа после записи; сообщение результата действия | [GAP-019](#gap-019-обработчик-после-команды-и-область-команды) |
| Старт с выбора класса, «Начать заново» после смерти | [GAP-015](#gap-015-старт-и-перезапуск-новой-игры-из-authoring) |
| Инвентарь «предмет → количество», флаги игрока | [GAP-008](#gap-008-поля-коллекции-и-целостность-ссылок-на-экземпляры) |
| Дикарка — экземпляр, созданный при встрече и удаляемый после продажи, побега или неудачной охоты | удаление персонажа — [GAP-018](#gap-018-имя-экземпляра-и-удаление-персонажа-из-authoring), [GAP-004](#gap-004-операции-с-runtime-instance-не-actor-kind) |
| Способы захвата, облики, шкалы описаний, фазы времени, ассортимент магазина данными | [GAP-010](#gap-010-перебор-definitions-по-kind-в-authoring) |
| Описание черты словом внутри фразы, имя героя в репликах | [GAP-011](#gap-011-локализуемые-аргументы-textspec) |
| Зона, фоны и эффекты входа локаций; характеристики персонажей в Definition | [GAP-012](#gap-012-поля-definition-из-блока-расширения-пакета) |
| Specs и эталонные прогоны игры рядом с пакетом | [GAP-016](#gap-016-место-specs-и-эталонных-прогонов-пакета) |
| Снятие заглушки `rh` | [GAP-017](#gap-017-тесты-движка-опираются-на-заглушку-rh) |

Реакция на событие не выполняется в самом подписчике: по [Commands and Events](../Architecture/CommandsAndEvents.md) подписчик работает при закрытом mutation window, а `game.random` пишет состояние потока в canonical `meta.prng`. Поэтому подписчик ставит в очередь команду реакции, а условия, бросок и запуск режима выполняет она. Это следствие contract, а не пробел.

## Временные игровые режимы

Источник: обсуждение архитектуры временных режимов (бой, торговля, диалог) от 2026-10-17. Принятое направление:

- Location — режим по умолчанию; одновременно активен не более одного временного режима.
- Состояние режима (Mode Run) не сохраняется: после load или restart открывается Location по `location_id` игрока.
- Команды режима — обычные `commands.*` с валидаторами, отказами и action bindings; отдельного диспетчера `handle(state, command, args)` нет. Общий контракт режима: `start`, `view`, `finish`, `cancel`.
- Политика записи в canonical state задаётся каждым режимом: торговля применяет каждую сделку сразу; диалог применяет последствия выбора сразу, временными остаются узел, переменные и журнал; бой применяет затраты и исход одной командой при завершении ([RH Gameplay Architecture § 5.11–5.14](../RH/GameplayArchitecture.md#511-временные-режимы)). Случайность режима — run-поток по [ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md).
- Состояние режима транзакционно вместе с командой: при `fail()` или ошибке оно восстанавливается. Сегодня `AuthoringFailAfterMutation` отслеживает только canonical `write_revision`, и `fail()` после изменения `local`-таблицы её не откатывает.
- Сохранение при активном режиме отказывается typed refusal `core:error.mode.active` ([ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md)); исходное направление «не запрещать, скрыть кнопку» отвергнуто.

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

- Правила работают с любым персонажем и получают его параметром. Набор характеристик объявлен один на всех Actor; разные виды персонажей используют разные поля (уточнено 2026-10-18).
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

- **Потребность:** общий интерфейс игрока показывает значения, выбранные игрой: имя и класс, золото, время суток, полоски с порогом (усталость), текстовые состояния (голод), роль-добычу, инвентарь с количеством и действиями, сообщения результата последней команды. Панели живут в отдельном шаблоне общего интерфейса слоя `core_interface` (`textsystem:screen.core_interface`), а не внутри экрана локации; экран локации получает от игры фон, выбранный по состоянию локации.
- **Сейчас:** `location_presenter` жёстко берёт золото и выносливость (`get_gold`, `get_stamina`), одну полоску `stamina` с делением на 100, предметы из `game.state.item_instances`, `effects = {}` и день из `game.state.meta.day`.
- **Требуется:**
  - authoring-способ для пакета игры задать значения панелей общего интерфейса (meters с произвольным максимумом и порогом, текстовые состояния, items с количеством и действиями, основной ресурс, время, сообщения команды) и `resource_id` фона экрана локации; `textsystem` не знает имён характеристик `rh`;
  - перенос панелей статуса из шаблона экрана локации в шаблон общего интерфейса — изменение шаблона, классифицируется по [Compatibility Policy](../Architecture/CompatibilityPolicy.md); сам шаблон готовится вместе с экранами.
- **Затрагивает:** [Screen Templates](../UI/ScreenTemplates.md), [Presentation Authoring Reference](../Authoring/PresentationAuthoringReference.md), `GameData/textsystem/scripts/presentation/location_presenter.lua`.
- **Статус:** open.

### GAP-008. Поля-коллекции и целостность ссылок на экземпляры

- **Потребность:** роли-множества (`player.companions`), списки участников игромеханических моделей конца хода, ограничение кандидата на роль (например, только персонаж с тегом `dog`). Флаги игрока и NPC, по которым ветвятся диалоги и реакции: имена флагов задаются в данных диалога, поэтому объявлять каждый флаг отдельным полем в Lua противоречит принципу «сначала данные».
- **Сейчас:** `Scripts/authoring/field.lua` даёт только скалярные constructors, `enum`, `ref_definition` и `ref_instance`; constructor списка нет, хотя `Scripts/authoring/properties.lua` уже обрабатывает kind `array`. Проверка висячих ссылок на экземпляры в `Scripts/runtime/state_validator.lua` охватывает только `owner_id` предметов и `meta.player_actor_id`; поле-роль на удалённого персонажа не проверяется. Ограничения на кандидата у `ref_instance` нет.
- **Требуется:**
  - `field.list(item_descriptor, opts)`;
  - поле-словарь с типизированными ключами и значениями, где ключ — Stable ID Definition указанного kind, а значение — boolean или integer: флаги (`rh:flag.met_merchant` → boolean) и инвентарь (`rh:item.food.supplies` → количество); ключ проверяется по repository, поэтому опечатка в данных ловится при сборке и чтении, а не молча создаёт новый ключ;
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

## Авторский слой, итог команды и общий интерфейс

Источник: решение от 2026-10-17 переписать `rh` с нуля, удалив заглушку, и проектирование игры «Охотник» 2026-10-18. Пробелы ниже мешают пакету игры целиком жить на authoring surface, проверяться своими specs и собирать экран из общего интерфейса.

### GAP-014. Поля и методы prototype на Definitions

- **Потребность:** runtime-state поля Definition (стадия и флаги квеста, постоянный запас магазина) и методы на Definitions собственных kinds `rh`.
- **Сейчас:** prototype `Quest`, `Location` или пользовательский PascalCase регистрирует field descriptors под своим именем (`Scripts/authoring/context.lua`, `create_entity_prototype_proxy`), а wrapper Definition ищет schema по discriminator или по kind в нижнем регистре (`Scripts/authoring/properties.lua`, `wrap_definition`), поэтому объявленное поле не находится: после `Quest.stage = field.non_negative_integer()` чтение `stage` у Definition квеста возвращает `nil`, запись отклоняется. Соответствие имени prototype многословному kind (`TurnPhase` ↔ `turn_phase`) не определено.
- **Требуется:** field descriptors и методы prototype применяются к Definitions своего kind через `def.<kind>(name)`; определённое соответствие имени prototype и kind; проверка через production-путь wrapper Definition.
- **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Lua Gameplay Reference](../Authoring/LuaGameplayReference.md).
- **Приоритет:** низкий: «Охотник» не использует квесты и постоянный запас.
- **Статус:** open.

### GAP-015. Старт и перезапуск новой игры из authoring

- **Потребность:** новая игра начинается стартовой командой пакета по данным сценария (у «Охотника» — сцена вступления с выбором класса); сцена конца игры перезапускает игру кнопкой «Начать заново», и новая игра получает новое зерно случайности. Главного меню пока нет: при запуске сразу начинается игра.
- **Сейчас:** [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md) даёт пакету `start` hook module; у authoring module возвращаемое значение игнорируется, поэтому `rh` стартует обычным module `runtime/session_start.lua` через programmer API и определяет новую игру по отсутствию игрока. Из Lua доступны только `core:command.session.save` и `core:command.session.load`; перезапуск сессии (`Restart`) копирует зафиксированный стартовый descriptor вместе с `seed_hex`, поэтому повторяет броски прошлой игры.
- **Требуется:**
  - authoring-объявление стартовой команды пакета, которую lifecycle вызывает только при новой игре; признак «новая игра» определяет lifecycle;
  - системная команда перезапуска новой игры, доступная из Lua как binding, с новым зерном случайности; отказ при активной операции lifecycle — typed.
- **Затрагивает:** [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md#deterministic-random-streams).
- **Приоритет:** блокирует старт и конец игры «Охотника».
- **Статус:** open.

### GAP-016. Место specs и эталонных прогонов пакета

- **Потребность:** specs механик, обходы контента и эталонные прогоны `rh` ([RH Gameplay Architecture § 9](../RH/GameplayArchitecture.md#9-проверка)) лежат рядом с пакетом и исполняются на реальном пакете и его зависимостях.
- **Сейчас:** spec runner находит specs только в `Tests/Lua/<subtree>/`, соответствие subtree уровню пакетов задано списком в C++ (`Source/GV2TestSupport/Private/LuaSpecRunner.cpp`, `GetAllSubtreeTierConfigs`). Эталонные прогоны регистрируются вручную в `Headless/CMakeLists.txt` и закреплены за корпусом движка.
- **Требуется:** каталог specs пакета, который runner находит сам; эталонные прогоны пакета рядом с ним без правки CMake, сверяемые в headless и UE; классификация таких проверок по [ADR-0046](../ADR/0046-test-content-coupling-boundary.md) как тестов игры.
- **Затрагивает:** [Add Lua Spec](../Guides/AddLuaSpec.md), [Build and Tooling](../Architecture/BuildAndTooling.md), [Headless Simulation](../Architecture/HeadlessSimulationContract.md).
- **Приоритет:** блокирует стратегию проверки `rh`.
- **Статус:** open.

### GAP-017. Тесты движка опираются на заглушку `rh`

- **Потребность:** удалить заглушку `rh` и написать пакет заново.
- **Сейчас:** Lua specs движка, цели `Tools/Content` и smoke-тест опираются на содержимое заглушки (`STATUS-028` в [Implementation Status](ImplementationStatus.md)).
- **Требуется:** перевести проверки движка на фикстурный пакет до удаления заглушки — задача EGC-27 [плана](../Plans/EngineGapClosure/Content.md#egc-27-отвязать-регрессию-движка-от-пакета-rh).
- **Статус:** open.

### GAP-018. Имя экземпляра и удаление персонажа из authoring

- **Потребность:** типовой NPC создаётся, когда нужен состоянию, и удаляется, когда больше не нужен: дикарка «Охотника» создаётся при встрече и удаляется после продажи, побега или неудачной охоты. Контенту нужна и постоянная ссылка на конкретный сгенерированный экземпляр.
- **Сейчас:** authoring `instances` даёт только `register_kind` и `create`; удаление персонажа с отказом при ссылках есть только в runtime registry (`Scripts/runtime/actor_registry.lua`, `remove`). Instance ID выдаёт allocator, поэтому заранее он неизвестен, а `actor(name)` требует ровно один экземпляр Definition.
- **Требуется:** удаление персонажа из authoring с политикой ссылок GAP-008 — задачей EGC-08 вместе с экземплярами других kinds; постоянное имя экземпляра, задаваемое при создании, и поиск по нему.
- **Затрагивает:** [Stable ID Specification](../Architecture/StableIDSpecification.md#persistent-instance-ids), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md).
- **Приоритет:** удаление блокирует добычу «Охотника»; имя экземпляра — низкий.
- **Статус:** open.

### GAP-019. Обработчик после команды и область команды

- **Потребность:** исход, наступающий после записи, проверяется после каждой успешной команды пакета: смерть от усталости, победа по золоту, переход в сцену по состоянию — и может начать режим той же командой. Данные одной команды — сообщения результата («Никого не нашли», «Дикарка убежала») и отложенный исход, поставленный эффектом, — не попадают в canonical state, а сообщения доходят до презентации и показываются панелью «последнее событие» общего интерфейса.
- **Сейчас:** у authoring surface нет обработчика пакета после команды: подписчик `on()` работает после commit при закрытом окне мутации и может только поставить отложенную команду, которая исполняется отдельной dispatch. Хранилища, живущего одну команду и доступного эффектам, Services и обработчику, нет. Презентация строится из canonical state и run активного режима, канала для данных команды у неё нет. Вызывать проверку вручную в каждой команде — утверждение без перечислителя.
- **Требуется:**
  - объявление обработчика пакета после команды, исполняемого внутри окна мутации той же команды после её handler, до решения транзакции и публикации фактов; ошибка обработчика откатывает команду по правилам [ADR-0050](../ADR/0050-transactional-mutation-window.md);
  - область команды: таблица portable values, доступная handler, эффектам, Services и обработчику, уничтожаемая после команды, вне state hash и save;
  - канал сообщений команды в презентацию: построитель экрана получает сообщения последней команды; новая команда их заменяет; в save и hash они не входят.
- **Затрагивает:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [UI Document](../UI/UIDocumentAndReconciliation.md), [ADR-0050](../ADR/0050-transactional-mutation-window.md), [ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md).
- **Приоритет:** блокирует смерть, победу и сообщения результата «Охотника».
- **Статус:** open.

### GAP-020. Общий интерфейс при смене сменной части экрана

- **Потребность:** экран собирается из общего интерфейса (слой `core_interface`) и панелей конкретного экрана; общие значения не дублируются на экранах режимов. Режим при запуске решает, оставить общий интерфейс на время режима или скрыть (вступление до выбора класса скрывает).
- **Сейчас:** по [ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md) `view` возвращает только route-экран; слои, открытые командами режима, получают owner = режим и удаляются при выходе (EGC-17). Правила для слоя, опубликованного вне режима и живущего через смену route, нет; объявить его скрытие на время режима нельзя. Панели статуса сегодня живут внутри шаблона экрана локации.
- **Требуется:** слой `core_interface`, опубликованный построителем по умолчанию, сохраняется при входе в режим и выходе из него тем же widget; режим объявляет при старте `core_interface: true | false`, и выбор держится до конца режима; вход и выход — по одной ревизии. Решение фиксируется новым ADR, дополняющим ADR-0051.
- **Затрагивает:** [UI Document](../UI/UIDocumentAndReconciliation.md), [Screen Templates](../UI/ScreenTemplates.md), [ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md).
- **Приоритет:** блокирует сборку экранов «Охотника».
- **Статус:** open.
