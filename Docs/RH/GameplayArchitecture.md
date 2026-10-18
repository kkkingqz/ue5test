---
title: RH Gameplay Architecture
status: draft
version: 0.3
updated: 2026-10-18
depends_on:
  - ../Architecture/AuthoringSurfaceContract.md
  - ../Architecture/CommandsAndEvents.md
  - ../Architecture/CanonicalStateAndSave.md
  - ../Architecture/GlossaryAndNaming.md
  - ../UI/SemanticInput.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../Status/EngineGaps.md
  - ../Status/ScreenRequests.md
decisions:
  - ../ADR/0026-core-and-gameplay-ownership.md
  - ../ADR/0027-designer-lua-authoring-layer.md
  - ../ADR/0050-transactional-mutation-window.md
  - ../ADR/0051-temporary-modes-and-presentation-routing.md
---

# Архитектура геймплея RH

> **Владеет:** устройством пакета `rh`: слоями и связями файлов, моделью игровых данных, общими механиками (характеристики, словари условий и эффектов, конвейер взаимодействия, итог команды, время, перемещение, персонажи, роли), общими модулями сцен (диалог, бой, торговля), сборкой экрана из общего интерфейса и панелей экрана, стратегией проверки.
> **Не владеет:** правилами конкретной игры — формулами, классами, предметами, локациями «Охотника» ([Hunter Gameplay](HunterGameplay.md)); механизмами движка (`core`, `textsystem`), командами, событиями, режимами и состоянием как таковыми — их семантику задают [Commands and Events](../Architecture/CommandsAndEvents.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md) и [ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md).
> **Статус:** `draft` — принятая архитектура до реализации; становится `normative`, когда механики реализованы и проверены.

## 1. Назначение и границы

`rh` — однопользовательская текстовая RPG: мир из локаций, у каждой картинка, описание и меню взаимодействий; сцены с выбором, бои, торговля и мини-игры идут временными режимами. Первая игра пакета — «Охотник» ([Hunter Gameplay](HunterGameplay.md)).

Архитектура шире, чем нужно первой игре: механизмы, которые «Охотник» не использует (выносливость, команда конца хода и отчёт, сознание, роли-списки, модели NPC, процессы, квесты, здоровье и ходы противника в бою, модификаторы цен и запас в торговле), описаны здесь как точки расширения и реализуются, когда появляется контент, которому они нужны.

- Весь геймплей пишется в `GameData/rh/` на authoring surface ([Authoring Surface](../Architecture/AuthoringSurfaceContract.md)).
- Общие механизмы, которых не хватает, относятся к `textsystem` и `core` и записываются в [Engine Gaps](../Status/EngineGaps.md); обход через `game.*` (кроме `game.random`) или сырое `game.state` запрещён.
- Экраны и UI-поля, которых не хватает, записываются в [Screen Requests](../Status/ScreenRequests.md).
- Тексты и картинки контента заводятся заглушками ID в Definitions, их содержание описывается заявками на ресурсы.

## 2. Слои пакета и связи

### 2.1. Раскладка

```text
GameData/rh/
  definitions/          Definitions rh: <kind>[_<группа>].json5
  schemas/              schemas собственных kinds и расширений
  scripts/authoring/
    model/              поля и методы сущностей: характеристики, инвентарь, время
    rules/
      formulas/         чистые функции: формулы, ядра режимов
      *.lua             Services: словари, конвейер, время, перемещение, модули сцен
    modes/              оболочки режимов: start, view, команды режима
    presentation/       значения общего интерфейса и экрана локации
    content/            уникальное поведение конкретного контента
```

| Слой | Содержит | Ограничение |
|---|---|---|
| `model` | prototype-поля и методы `Actor`, `Item`, `World` | не зависит от остальных слоёв |
| `rules/formulas` | чистые функции: получают значения и `rng`, возвращают результат | не читает и не пишет canonical state |
| `rules` | Services механик и модулей сцен | не содержит ID конкретного контента |
| `modes` | `modes.<name>` и команды режимов | правила берёт из `rules` |
| `presentation` | значения полей экрана | только читает state |
| `content` | поведение конкретных локаций, NPC, сцен | появление файла — сигнал, что не хватает типа условия, эффекта или механики |

Зависимости: `model ← rules/formulas ← rules ← modes ← content`; `presentation` читает `model`, `rules` и run активного режима.

### 2.2. Связи между файлами

Authoring-файлы не вызывают `require` друг друга.

| Связь | Как | Когда |
|---|---|---|
| Методы сущностей | `function Actor:stat(name)` | общие для пакета |
| Services | полный Stable ID: `services["rh:service.interactions"]`; имя Service — один сегмент | правило, нужное в нескольких файлах |
| Commands | `commands["rh:command.perform"]:later(...)` | отложенное действие, реакция |
| Events | `emit`, `on` | факт после записи |

**Термин «действие».** [Glossary](../Architecture/GlossaryAndNaming.md#запрещённые-неоднозначности) запрещает «gameplay action»: изменение мира — Command, а kind `action` — Semantic Action ([Semantic Input](../UI/SemanticInput.md)). Здесь пункт меню — Definition kind `interaction`; все пункты связаны с одной Semantic Action `rh:action.interact`, которая вызывает Command `rh:command.perform`.

### 2.3. Проверка отдельно от записи

Механика проверяет всё до первой записи: `check` возвращает причину отказа без записи, `apply` пишет. Причина — `{ key, params }`; из ключа получаются error `rh:error.<key>` и текст `rh:text.error.<key>`. Меню и варианты сцен берут доступность и причину из того же `check`, что и команда. `fail()` после записи запрещён ([Authoring Surface](../Architecture/AuthoringSurfaceContract.md#execution-scope-and-attribution)); исход, наступающий после записи (смерть, победа), — не отказ, а работа обработчика после команды (§ 5.4).

## 3. Инварианты

- Набор характеристик объявлен в коде одной таблицей и даёт поля всем Actor; разные виды персонажей используют разные поля. Правило получает персонажа параметром.
- В canonical state хранится только то, что нельзя вычислить: итоговые характеристики, доступность взаимодействия и причина недоступности не сохраняются.
- Взаимодействие, переход, вариант сцены, предложение магазина, действие боя, фаза времени описываются данными; Lua нужен для нового типа условия, эффекта, проверки или источника, новой механики или уникального поведения контента.
- Данные `rh` хранятся в собственных kinds; расширения kinds других пакетов минимальны.
- Временные режимы не сохраняются; сохранение при активном режиме отказывается ([ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md)).
- У NPC нет собственных ходов: NPC действует в ответ на взаимодействие игрока или в фазе времени; фаза обращается к NPC через роли и списки участников, перебор всех NPC мира запрещён.
- Общие значения игрока показывает общий интерфейс; экраны локации и режимов их не дублируют.
- Механики и модули сцен не содержат ID конкретного контента.

## 4. Модель данных

### 4.1. Kinds

| Kind | Владелец schema | Содержит |
|---|---|---|
| `interaction` | `rh` | текст, условия, эффекты, источник меню (игрок, инвентарь, локация, переход), необязательная стоимость в выносливости |
| `location` | `textsystem` + расширение `rh` | заголовок, экраны, переходы; в расширении — зона, фоны по состоянию, взаимодействия, эффекты входа |
| `actor` | `core` + расширение `rh` | имя, портрет, теги; у уникального NPC — значения характеристик; у шаблона типового NPC — параметры генерации |
| `item` | `rh` | категория, цена, `stackable`, модификаторы характеристик, условия покупки, иконка |
| `dialogue` | `rh` | граф сцены с выбором (§ 5.12) |
| `combat` | `rh` | бой (§ 5.13) |
| `shop` | `rh` | магазин (§ 5.14) |
| `time_phase` | `rh` | `order`, Service фазы — исполняется на каждый сдвиг времени |
| `flag` | `rh` | объявление флага |
| `scenario` | `rh` | стартовое состояние |
| `config` | `rh` | пороги и параметры формул |
| `quest`, `process` | `rh` | стадии квеста; рецепт процесса — точки расширения, первой игрой не используются |

Игра добавляет свои kinds (у «Охотника» — классы, способы захвата, облики, шкалы описаний). Новый kind добавляется вместе со schema ([Stable ID Specification](../Architecture/StableIDSpecification.md#kind-registry)). Поля Definitions собственных kinds читаются через `def.<kind>(name)`; поля расширений чужих kinds — после [GAP-012](../Status/EngineGaps.md#gap-012-поля-definition-из-блока-расширения-пакета); перебор Definitions kind — после [GAP-010](../Status/EngineGaps.md#gap-010-перебор-definitions-по-kind-в-authoring).

### 4.2. Что где хранится

| Сущность | Definition (статично) | Canonical state | Run режима | Вычисляется |
|---|---|---|---|---|
| Персонаж | имя, портрет, теги, характеристики уникального NPC, параметры генерации шаблона | характеристики сгенерированного или изменённого персонажа, ресурсы (усталость, голод, выносливость), локация, флаги, поля-роли, инвентарь | — | итоговые характеристики |
| Локация | заголовок, фоны, взаимодействия, эффекты входа | изменения состояния локации | — | выбранный фон и вариант описания |
| Мир | — | время суток, номер хода | — | — |
| Сцена диалога | граф | эффекты выбора | текущий узел, переменные, журнал | доступность вариантов |
| Бой | противники, действия, исходы | итог — при завершении | раунд, здоровье, накопленные затраты | — |
| Магазин | предложения, цены | покупки и продажи — сразу | запас на визит | цены и доступность |

### 4.3. Характеристики и модификаторы

```lua
-- model/stats.lua — единственный перечень характеристик пакета
STATS = {
    stealth  = {},           attack  = {},           pursuit  = {},
    looks    = { 1, 6 },     height  = { 1, 3 },     figure   = { 1, 7 },
    agility  = { 1, 7 },     wits    = { 1, 7 },     maturity = { 1, 4 },
}

for name, range in pairs(STATS) do
    Actor[name] = field.integer({ min = range[1], max = range[2], nullable = true })
end

function Actor:base(name) return self[name] or 0 end

function Actor:stat(name)                     -- итог: база плюс модификаторы
    local value = self:base(name)
    for item, count in self:inventory_items() do
        local bonus = item.modifiers and item.modifiers[name]
        if bonus then value = value + bonus end
    end
    return value
end
```

База — значение экземпляра, если оно задано, иначе значение Definition. Модификаторы сейчас дают предметы; модификаторы ролей и эффектов добавляются тем же методом. Правила читают только `stat(name)`; запись идёт в базу.

### 4.4. Соглашения об ID

| Что | Форма | Пример |
|---|---|---|
| Definition | `rh:<kind>.<группа>.<имя>` | `rh:location.rome.square` |
| Текст поля Definition | `rh:text.<kind>.<группа>.<имя>.<поле>` | `rh:text.interaction.woods.hunt.title` |
| Причина отказа | error `rh:error.<механика>.<причина>`, текст `rh:text.error.<механика>.<причина>` | `rh:error.shop.not_enough_gold` |
| Сообщение результата | `rh:text.message.<механика>.<ключ>` | `rh:text.message.hunt.nobody` |
| Ресурс | `rh:resource.<kind>.<группа>.<имя>.<назначение>` | `rh:resource.location.rome.square.background` |
| Service словаря | `rh:service.<словарь>_<тип>` | `rh:service.condition_has_item` |
| Поток случайности | `rh:random_stream.<подсистема>` | `rh:random_stream.hunt` |

Сегмент Stable ID начинается с буквы, поэтому номер из исходников игры идёт суффиксом: `rh:text.dance.step_301`.

## 5. Механики

### 5.1. Формулы

Формулы — именованные чистые функции в `rules/formulas/`; параметры — в Definition `config`. Функция получает значения и `rng(a, b)`, поэтому проверяется без сессии и без режима.

### 5.2. Словари

Расширяемые наборы — Services с коротким типом в данных; новый тип, в том числе из мода, — новый Service без правки механики.

| Словарь | Service | Методы | Кто использует |
|---|---|---|---|
| Условия | `rh:service.condition_<type>` | `check(ctx, params) → bool`, `reason(ctx, params) → { key, params }` | взаимодействия, варианты диалога, действия боя, предложения магазина, фазы |
| Эффекты | `rh:service.effect_<type>` | `apply(ctx, params)`, необязательный `check(ctx, params) → reason?` | взаимодействия, варианты, исходы, действия магазина |
| Проверки | `rh:service.check_<type>` | `roll(ctx, run, params) → bool` | диалог, бой |
| Источники вариантов | `rh:service.choices_<source>` | `list(ctx, run, params) → choices` | диалог |
| Источники текста | `rh:service.text_<source>` | `lines(ctx, run, params) → TextSpec[]` | диалог |
| Источники картинки | `rh:service.image_<source>` | `resource(ctx, run, params) → resource_id` | диалог |
| Источники предложений | `rh:service.offers_<source>` | `list(ctx, run, params) → offers` | магазин |
| Формулы цены | `rh:service.price_<formula>` | `quote(ctx, offer)`, `settle(ctx, offer)` | магазин |
| Проверка ввода | `rh:service.input_<validate>` | `check(value) → reason?` | диалог |

```json5
requires: [
  { type: "has_item", item: "rh:item.weapon.sword" },
  { type: "time_of_day_in", values: ["morning"] },
],
effects: [
  { type: "gain_fatigue" },
  { type: "advance_time" },
],
```

Список условий или эффектов в schema — `union` по полю `type` ([Definition Envelope and Schema Rules](../Architecture/DefinitionEnvelopeAndSchemaRules.md)): параметры каждого типа проверяются при сборке repository. `ctx` — исполнитель, цель, локация, run активного режима (переменные и `subject` сцены) и область команды (§ 5.4).

### 5.3. Конвейер взаимодействия

```lua
-- rules/interactions.lua
services.interactions = {
    check = function(ctx)                          -- без записи; меню берёт отсюда причину
        if modes.active() ~= nil then return reason("mode.busy") end
        return conditions.first_failure(ctx, ctx.interaction.requires)
            or stamina_failure(ctx)                -- если взаимодействие объявило стоимость
            or effects.first_precondition_failure(ctx, ctx.interaction.effects)
    end,
    apply = function(ctx)
        pay_stamina(ctx)
        effects.apply_all(ctx, ctx.interaction.effects)
        emit("interaction_completed", { interaction = ctx.interaction })
    end,
}

commands.perform = function(interaction_id, target)
    local ctx = context(player, def.interaction(interaction_id), target)
    local failure = services.interactions.check(ctx)
    if failure then fail(failure.key, failure.params) end
    services.interactions.apply(ctx)
end
```

Новое взаимодействие — только Definition. Приказ спутнику — та же Command с аргументом `performer` (вводится вместе со спутниками). Выносливость — необязательная стоимость взаимодействия: «Охотник» её не объявляет.

### 5.4. Итог команды

Исход, который наступает после записи, проверяет обработчик пакета после каждой успешной команды: смерть, победа, переход в другую сцену по состоянию. Данные одной команды — сообщения результата и отложенные исходы — живут в области команды и в state не попадают. И обработчик, и область команды — механизм движка ([GAP-019](../Status/EngineGaps.md#gap-019-обработчик-после-команды-и-область-команды)).

```lua
-- rules/outcome.lua
after_command(function(scope)
    local death = scope.death or rules_of_game.death_cause(player)   -- правило игры
    if death then
        if modes.active() then modes.finish_active({ outcome = "died" }) end
        start_dialogue(scope, rules_of_game.game_over_dialogue, { reason = death })
        return
    end
    if modes.active() == nil then rules_of_game.check_victory(scope) end
end)

function say(ctx, key, params)                    -- сообщение результата
    table.insert(ctx.scope.messages, { text = "rh:text.message." .. key, params = params })
end
```

Сообщения последней команды показывает панель «последнее событие» общего интерфейса (§ 7); следующая команда их заменяет. Журнал внутри сцены диалога или боя хранит run режима.

### 5.5. Время

Время мира — `world.time_of_day` и номер хода. Сдвиг делает эффект `advance_time`; на каждый сдвиг Definitions `time_phase` исполняются по возрастанию `order` (при равенстве — по Stable ID): мод добавляет фазу новой Definition и отключает её переопределением. Рекомендуемый шаг `order` — 100.

```lua
services.time = {
    advance = function(ctx, steps)
        for _ = 1, steps or 1 do
            world.time_of_day = next_period(world.time_of_day)
            for _, phase in ipairs(time_phases_by_order()) do      -- GAP-010
                services[phase.service].run(ctx)
            end
        end
    end,
}
```

Две формы игры используют один механизм:

- **время от действий** («Охотник»): взаимодействия перечисляют `advance_time` в эффектах; кнопки конца хода нет;
- **ход** (точка расширения): Command `rh:command.turn.end` доступна всегда, сдвигает время на длительность хода из `config`, собирает записи фаз и при непустом списке запускает режим отчёта о ходе.

### 5.6. Перемещение

Переход — взаимодействие с эффектом `travel`; источник «переходы» меню строится из связей локации. Условия перехода — из словаря; эффекты входа в локацию — из её расширения. После перехода публикуется `rh:event.travel_completed`.

### 5.7. Персонажи: уникальные и типовые

- **Уникальный NPC** — Definition `actor` и ровно один экземпляр, созданный сценарием старта; код обращается к нему `actor("npc.<имя>")`.
- **Типовой NPC** — экземпляр, сгенерированный из шаблона формулой генерации; броски — поток генерации; значения хранятся в экземпляре. Экземпляр создаётся, когда персонаж впервые нужен состоянию — например, при встрече, чтобы под его значения подобрать картинки, — и удаляется, когда больше не нужен ([GAP-018](../Status/EngineGaps.md#gap-018-имя-экземпляра-и-удаление-персонажа-из-authoring)).
- **Обращение к конкретному экземпляру из кода** — через роль (поле-ссылку, § 5.8) или через имя экземпляра, заданное при создании (GAP-018).

### 5.8. Роли

Роль — поле-ссылка на экземпляр (`Actor.catch = field.ref_instance("actor", { nullable = true })`) на персонаже или мире. Общие обработчики работают с ролью, а не с ID. Роли-списки, ограничение кандидата и контроль висячих ссылок — [GAP-008](../Status/EngineGaps.md#gap-008-поля-коллекции-и-целостность-ссылок-на-экземпляры).

### 5.9. Сознание, модели NPC, процессы, квесты

Точки расширения, первой игрой не используемые:

- **сознание** — первый шаг конвейера `performer:can_act()`; бессознательный персонаж не действует и не участвует в фазах;
- **модель NPC** — фаза времени с участниками из ролей; исполняет взаимодействие участника и возвращает записи;
- **процесс** — рецепт и экземпляр с прогрессом; вклад — взаимодействие игрока или спутника ([GAP-004](../Status/EngineGaps.md#gap-004-операции-с-runtime-instance-не-actor-kind));
- **квест** — уникальная Definition со стадией и флагами в runtime-state полях ([GAP-014](../Status/EngineGaps.md#gap-014-поля-и-методы-prototype-на-definitions)); переходы — словарём условий, продвижение — по Events.

### 5.10. Events

Словарь фактов `rh`: `rh:event.interaction_completed`, `rh:event.travel_completed`, `rh:event.time_advanced`, `rh:event.turn_ended`, `rh:event.actor_lost_consciousness`, `rh:event.actor_regained_consciousness`, `rh:event.process_completed`, `rh:event.quest_stage_changed`; режимы публикуют `core:event.mode.*`. Подписчик работает при закрытом mutation window и только ставит в очередь команду реакции ([Commands and Events](../Architecture/CommandsAndEvents.md)).

### 5.11. Временные режимы

Режим — по [ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md): один активный, без вложенности; `view` возвращает только экран сменной части; run транзакционен вместе с командой; сохранение при активном режиме отказывается; завершить один режим и начать другой в одной команде можно.

Режим делится на две части: **чистое ядро** в `rules/formulas/` (расчёт без state) и **оболочку** в `modes/` (`start`, `view`, команды, run). Общие модули сцен — диалог, бой, торговля (§ 5.12–5.14) — покрывают большинство сцен; игра добавляет свои режимы, когда сцене нужно собственное ядро (у «Охотника» — танец).

Каждый режим при запуске решает, показывать ли общий интерфейс: поле `core_interface` в Definition сцены, по умолчанию `true`; выбор хранится в run до конца режима ([GAP-020](../Status/EngineGaps.md#gap-020-общий-интерфейс-при-смене-сменной-части-экрана)).

### 5.12. Диалог (`rh:mode.dialogue`)

Общий модуль сцен с выбором: разговоры, вступление, охота, осмотр, конец игры. Возможности: ветвление по условиям, переменные сцены, броски, текст и дополнительное описание на каждую стадию с вариантами по условиям, динамические варианты и тексты, ввод значения, конечный узел без выбора.

| Поле Definition | Смысл |
|---|---|
| `start` | первый узел |
| `subject` | кто в центре сцены: `argument` (передан при старте), роль или нет; от него портрет, условия `subject_*`, источники текста |
| `nodes.<id>` | узел (стадия) |
| `outcomes.<исход>` | эффекты при завершении с этим исходом |
| `core_interface` | показывать общий интерфейс во время сцены; по умолчанию `true` |

| Поле узла | Смысл |
|---|---|
| `speaker` | нет — рассказчик; `{ actor }`; `{ subject: true }` |
| `image` | варианты `[{ when?, resource? \| subject_image? \| source? }]`, показывается первый подходящий; одиночный объект — вариант без условия |
| `text` | варианты `[{ when?, text? \| source? }]`, показывается первый подходящий |
| `description` | дополнительный текст стадии, такие же варианты, отдельный блок |
| `on_enter` | эффекты при входе в узел |
| `choices` | варианты; элемент с `source` разворачивается в список вариантов |
| `input` | вместо вариантов — ввод в модальном окне: `{ kind, validate, effect }` плюс переход |
| `terminal` | узел без выбора; режим остаётся открытым |

| Поле варианта и перехода | Смысл |
|---|---|
| `requires` | условия; не выполнены — вариант неактивен с причиной; `hide_unavailable: true` — скрыт |
| `effects` | эффекты по порядку; пишут canonical state сразу |
| `say` | запись в журнал сцены |
| `next`, `finish` | переход в узел; завершение с исходом |
| `branch` | `[{ when?, …переход }]` — первая подходящая ветка |
| `check`, `on_true`, `on_false` | бросок проверки и две ветки; переходы вкладываются |

Условия и эффекты модуля: `var_is`, `var_set`, `subject_*`; `set_var`, `take_subject`, `discard_subject`, `start_dialogue`, `start_combat`, `start_shop`.

```lua
-- modes/dialogue.lua
modes.dialogue = {
    -- run-поток случайности объявляется ниже: modes.dialogue.rng = field.random_stream()
    start = function(args)
        return { dialogue = args.dialogue, node = nil, subject = args.subject, vars = args.vars or {},
                 log = {}, core_interface = default_true(args.dialogue.core_interface) }
    end,
    view = function(run) return dialogue_view(run) end,
}

local function node(run) return run.dialogue.nodes[run.node] end

local function enter(ctx, run, id)
    run.node = id
    effects.apply_all(ctx, node(run).on_enter or {})
end

local function current_choices(ctx, run)
    local list = {}
    for _, c in ipairs(node(run).choices or {}) do
        if c.source then append(list, services["choices_" .. c.source].list(ctx, run, c.params))
        else table.insert(list, c) end
    end
    return list
end

local function go(ctx, run, t)                     -- переход; вложенные раскрываются
    if t.say then table.insert(run.log, t.say) end
    if t.check then
        local ok = services["check_" .. t.check.type].roll(ctx, run, t.check)
        return go(ctx, run, ok and t.on_true or t.on_false)
    elseif t.branch then
        for _, b in ipairs(t.branch) do
            if conditions.all(ctx, b.when) then return go(ctx, run, b) end
        end
    elseif t.finish then
        effects.apply_all(ctx, run.dialogue.outcomes[t.finish] or {})
        modes.dialogue:finish({ outcome = t.finish })
    elseif t.next then
        enter(ctx, run, t.next)
    end
end

services.effect_start_dialogue = { apply = function(ctx, p)
    local d = def.dialogue(p.dialogue)
    local run = modes.dialogue:start({ dialogue = d, subject = resolve_subject(ctx, d.subject, p.subject), vars = p.vars })
    enter(context(player, run), run, d.start)
end }

commands["dialogue.choose"] = function(choice_id)
    local run = modes.dialogue:require_active()
    local ctx = context(player, run)
    local choice = find_by_id(current_choices(ctx, run), choice_id)
    if choice == nil then fail("dialogue.unknown_choice") end
    local failure = conditions.first_failure(ctx, choice.requires)
    if failure then fail(failure.key, failure.params) end
    effects.apply_all(ctx, choice.effects or {})
    go(ctx, run, choice)
end

commands["dialogue.input"] = function(value)       -- значение поля ввода модального окна
    local run = modes.dialogue:require_active()
    local input = node(run).input
    if input == nil then fail("dialogue.no_input") end
    local failure = services["input_" .. input.validate].check(value)
    if failure then fail(failure.key, failure.params) end
    local ctx = context(player, run)
    effects.apply_all(ctx, { { type = input.effect, value = value } })
    go(ctx, run, input)
end
```

### 5.13. Бой (`rh:mode.combat`)

Общий модуль: стороны, раунды, действия игрока с условиями, затратами и проверками, поведение противника, здоровье и урон, альтернатива боя, исходы. Затраты действий и эффекты исхода применяются к canonical state одной командой при завершении боя.

| Поле Definition | Смысл |
|---|---|
| `enemies` | противники `{ key, actor }`; характеристики и здоровье — из Definition персонажа |
| `alternatives` | `[{ when, dialogue }]` — вместо боя сцена диалога, если условия выполнены |
| `intro` | варианты вступительного текста |
| `player_actions` | `id`, `text`, `requires`, `cost` (эффекты), переход: `check`/`on_true`/`on_false`, `damage`, `say`, `finish` |
| `enemy_behavior` | `none` \| `sequence` \| `random` и список действий противника той же формы |
| `defeat` | исходы по здоровью: `{ enemies: <исход>, player: <исход> }` |
| `outcomes.<исход>` | эффекты при завершении |
| `core_interface` | показывать общий интерфейс во время боя; по умолчанию `true` |

```lua
-- modes/combat.lua
modes.combat = {
    -- run-поток случайности: modes.combat.rng = field.random_stream()
    start = function(args)
        return { combat = args.combat, round = 1, hp = initial_hp(args.combat), pending = {}, log = {},
                 core_interface = default_true(args.combat.core_interface) }
    end,
    view = function(run) return combat_view(run) end,
}

services.effect_start_combat = { apply = function(ctx, p)
    local c = def.combat(p.combat)
    for _, alt in ipairs(c.alternatives or {}) do
        if conditions.all(ctx, alt.when) then
            return services.effect_start_dialogue.apply(ctx, { dialogue = alt.dialogue })
        end
    end
    modes.combat:start({ combat = c })
end }

local function finish_combat(ctx, run, outcome)
    effects.apply_all(ctx, run.pending)                   -- затраты действий
    effects.apply_all(ctx, run.combat.outcomes[outcome] or {})
    modes.combat:finish({ outcome = outcome })
end

local function resolve(ctx, run, t)                       -- true — бой завершён
    if t.check then
        local ok = services["check_" .. t.check.type].roll(ctx, run, t.check)
        return resolve(ctx, run, ok and t.on_true or t.on_false)
    end
    if t.damage then apply_damage(ctx, run, t.damage) end
    if t.say then table.insert(run.log, t.say) end
    if t.finish then finish_combat(ctx, run, t.finish); return true end
    return false
end

commands["combat.act"] = function(action_id)
    local run = modes.combat:require_active()
    local ctx = context(player, run)
    local action = find_by_id(run.combat.player_actions, action_id)
    local failure = conditions.first_failure(ctx, action.requires)
    if failure then fail(failure.key, failure.params) end
    append(run.pending, action.cost or {})
    if resolve(ctx, run, action) then return end
    local enemy_action = choose_enemy_action(run)
    if enemy_action and resolve(ctx, run, enemy_action) then return end
    local defeated = defeated_side(run)
    if defeated then return finish_combat(ctx, run, run.combat.defeat[defeated]) end
    run.round = run.round + 1
end
```

### 5.14. Торговля (`rh:mode.shop`)

Общий модуль: покупка и продажа, вкладки, динамический ассортимент из источников, цены с формулами и модификаторами по условиям, условия на предложения, запас, приветствие по условиям, действия магазина.

| Поле Definition | Смысл |
|---|---|
| `title`, `merchant` | название; имя и портрет торговца |
| `greeting` | варианты реплики |
| `tabs` | `{ id, title, offers }` |
| `offers` | предложения или источник `{ source, params }` |
| `actions` | кнопки магазина `{ id, text, requires, effects }` |
| `core_interface` | показывать общий интерфейс в магазине; по умолчанию `true` |

| Поле предложения | Смысл |
|---|---|
| `kind` | `buy` — игрок покупает; `sell` — игрок продаёт |
| `item` / `subject` | предмет или персонаж из роли |
| `price` | `{ base }` или `{ formula }`, плюс `modifiers: [{ when, multiply?, add? }]`; без поля — цена предмета |
| `requires`, `hide_unavailable` | условия, как у вариантов диалога |
| `stock` | `{ per_visit }` — в run; `{ persistent }` — runtime-state поле Definition магазина ([GAP-014](../Status/EngineGaps.md#gap-014-поля-и-методы-prototype-на-definitions)) |

Цена считается дважды: `quote` — показываемая, `settle` — в момент сделки; формула может давать разные значения (оценка без броска и продажа с броском). Количество для `stackable` вводится в модальном окне. Уникальный предмет второй раз не покупается — правило модуля.

```lua
-- rules/trade.lua
local function price_spec(offer) return offer.price or { base = def.item(offer.item).price } end

local function apply_modifiers(ctx, spec, value)
    for _, m in ipairs(spec.modifiers or {}) do
        if conditions.all(ctx, m.when) then value = math.floor(value * (m.multiply or 1) + (m.add or 0)) end
    end
    return value
end

function quote(ctx, offer)
    local spec = price_spec(offer)
    return apply_modifiers(ctx, spec, spec.formula and services["price_" .. spec.formula].quote(ctx, offer) or spec.base)
end

function settle_price(ctx, offer)
    local spec = price_spec(offer)
    return apply_modifiers(ctx, spec, spec.formula and services["price_" .. spec.formula].settle(ctx, offer) or spec.base)
end

services.shop = {
    check = function(ctx, run, offer, quantity)
        local failure = conditions.first_failure(ctx, offer.requires)
        if failure then return failure end
        if offer.kind == "buy" then
            local item = def.item(offer.item)
            if quantity < 1 or (not item.stackable and quantity ~= 1) then return reason("shop.bad_quantity") end
            if not item.stackable and player:has(item) then return reason("shop.already_owned") end
            if stock_left(run, offer) < quantity then return reason("shop.out_of_stock") end
            local total = quote(ctx, offer) * quantity
            if player.gold < total then return reason("shop.not_enough_gold", { price = total, gold = player.gold }) end
        elseif not player_has_offer_subject(offer) then
            return reason("shop.nothing_to_sell")
        end
    end,
    buy = function(ctx, run, offer, quantity)
        player.gold = player.gold - settle_price(ctx, offer) * quantity
        player:give(offer.item, quantity)
        take_stock(run, offer, quantity)
    end,
    sell = function(ctx, run, offer)
        player.gold = player.gold + settle_price(ctx, offer)
        remove_offer_subject(ctx, offer)
    end,
}

modes.shop = {
    start = function(args)
        return { shop = args.shop, stock = {}, core_interface = default_true(args.shop.core_interface) }
    end,
    view = function(run) return shop_view(run) end,
}

commands["shop.buy"] = function(offer_key, quantity)
    local run = modes.shop:require_active()
    local ctx = context(player, run)
    local offer = find_offer(ctx, run, offer_key)            -- ключ: <вкладка>.<предмет или роль>
    local failure = services.shop.check(ctx, run, offer, quantity)
    if failure then fail(failure.key, failure.params) end
    services.shop.buy(ctx, run, offer, quantity)
end

commands["shop.sell"] = function(offer_key) --[[ то же с services.shop.sell ]] end

commands["shop.action"] = function(action_id)                -- например, начать диалог
    local run = modes.shop:require_active()
    local ctx = context(player, run)
    local action = find_by_id(run.shop.actions, action_id)
    local failure = conditions.first_failure(ctx, action.requires)
    if failure then fail(failure.key, failure.params) end
    modes.shop:finish({ outcome = "action" })
    effects.apply_all(ctx, action.effects)                    -- второй режим в той же команде
end

commands["shop.leave"] = function()
    modes.shop:require_active()
    modes.shop:finish({ outcome = "left" })
end
```

### 5.15. Случайность

Вне режима — отдельный поток `game.random` на подсистему (`rh:random_stream.<подсистема>`): изменение одной подсистемы не сдвигает броски другой. Внутри режима — run-поток режима по [ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md): режим, в котором есть броски (диалог, бой, торговля, режимы игры), объявляет его полем run, зерно берётся один раз при старте, броски внутри режима не меняют канонический хеш, отказ внутри шага не тратит бросок. Проверки словаря `check_<type>` бросают из `run.rng`.

### 5.16. Старт новой игры

Стартовая команда пакета создаёт игрока и стартовое состояние по Definition `scenario` и запускает сцену вступления; состав стартового мира — данные. Вызов стартовой команды при новой игре и перезапуск игры из Lua с новым зерном случайности — механизм движка ([GAP-015](../Status/EngineGaps.md#gap-015-старт-и-перезапуск-новой-игры-из-authoring)).

## 6. Основные сценарии

```text
Кнопка взаимодействия → rh:action.interact → rh:command.perform(interaction, target)
  interactions.check: режим? → условия? → стоимость? → иначе fail(причина)
  interactions.apply: эффекты по порядку → rh:event.interaction_completed
  обработчик после команды: смерть? → сцена конца игры; победа? → сцена победы
  → presentation: общий интерфейс + экран локации, «последнее событие» — сообщения команды

Эффект start_dialogue → режим диалога → dialogue.choose / dialogue.input
  условия варианта → эффекты → переход (узел, ветка, бросок, завершение)
  завершение → эффекты исхода → режим закрыт или сменён другим в той же команде
```

## 7. Экран

Экран собирается из **общего интерфейса** и **панелей конкретного экрана**:

```text
Game Shell
  core_interface    textsystem:screen.core_interface   общий интерфейс игрока
  location_content  экран локации или экран активного режима
  modal_stack       ввод текста и числа
```

- **Общий интерфейс** — один экран слоя `core_interface`: статус, состояние, роль-добыча, инвентарь с действиями, «последнее событие». Значения панелей задаёт `rh` ([GAP-007](../Status/EngineGaps.md#gap-007-данные-панелей-статуса-из-пакета-игры)); `textsystem` владеет форматом. Во время режима интерфейс остаётся, если режим это выбрал (§ 5.11); действия инвентаря тогда неактивны с причиной `mode.busy`.
- **Экран локации** — `textsystem:screen.location`: фон, персонажи, описание, меню. Фон выбирает `rh` по состоянию локации и публикует `resource_id`. Меню собирается из источников: игрок, инвентарь, локация, переходы ([GAP-006](../Status/EngineGaps.md#gap-006-сборка-меню-локации-из-источников-действий)); недоступный пункт неактивен с причиной ([SR-001](../Status/ScreenRequests.md#sr-001-неактивный-пункт-меню-с-подсказкой-о-причине)).
- **Экраны режимов** — экран диалога, сцена с участниками (бой и мини-игры), магазин. Их собственные панели не повторяют общий интерфейс. Шаблоны экранов готовятся отдельно от этого документа.

## 8. Расширяемость и эволюция

- Сначала данные: локация, взаимодействие, сцена, бой, магазин, предмет, фаза добавляются без кода.
- Точки расширения для модов: Services словарей (§ 5.2), Definitions `time_phase`, Events.
- Definition переопределяется только целиком ([GameDataRepository Contract](../Architecture/GameDataRepositoryContract.md)); расширяемые наборы — отдельные Definitions, а не списки внутри одной.
- Механизм проектируется шире, чем первая игра: возможности модулей и точки расширения из § 1 описаны здесь, реализуются вместе с первым контентом-потребителем.

## 9. Проверка

Число тестов растёт с числом механик, а не с объёмом контента.

| Уровень | Что проверяет | Объём |
|---|---|---|
| Schemas данных | каждую Definition при сборке repository: поля, типы, `union` условий и эффектов, ссылки | schemas `rh`, без тестов |
| Обходы контента | один spec на точку расширения перебирает весь контент: типы условий, эффектов, проверок и источников существуют; переходы ведут в существующие локации и узлы; у каждого `condition_*` есть `check` и `reason` | около 10 |
| Specs модулей | диалог, бой, торговля — на синтетических Definitions, а не на контенте игры: варианты текста и описания, скрытый и неактивный вариант, вложенные переходы, переменные, ввод, конечный узел; альтернатива боя, затраты при завершении, урон и поражение, поведение противника; источники ассортимента, модификаторы, `quote` и `settle`, продажа персонажа, запас, действие магазина | 5–10 на модуль |
| Specs ядер и механик | чистые ядра и формулы; конвейер, итог команды, время — с отрицательным случаем | 3–5 на механику |
| Эталонные прогоны | headless-последовательность команд → эталонный state hash ([Regenerate Golden](../Guides/RegenerateGolden.md)) | 3–5 |

Перечислитель обходов — repository (все Definitions kind) и реестр Services, а не рукописный список. Место specs и эталонных прогонов пакета — [GAP-016](../Status/EngineGaps.md#gap-016-место-specs-и-эталонных-прогонов-пакета).
