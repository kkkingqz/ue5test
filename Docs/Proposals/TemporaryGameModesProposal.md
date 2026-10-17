---
title: Temporary Game Modes Proposal
status: draft
proposal_state: accepted_for_planning
version: 1.2
updated: 2026-10-17
depends_on:
  - ../Architecture/AuthoringSurfaceContract.md
  - ../Architecture/CommandsAndEvents.md
  - ../Architecture/CanonicalStateAndSave.md
  - ../Architecture/RuntimeFacadeAndRegistries.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../Status/EngineGaps.md
  - ../Status/ScreenRequests.md
  - MutationWindowTransactionalityProposal.md
  - ../ADR/0039-presentation-source-singleton.md
decisions:
  - ../ADR/0027-designer-lua-authoring-layer.md
  - ../ADR/0026-core-and-gameplay-ownership.md
  - ../ADR/0046-test-content-coupling-boundary.md
---

# Предложение по временным игровым режимам

> **Предлагает:** authoring-примитив `modes` для временных игровых режимов (магазин, диалог, танец, бой, отчёт о конце хода): несохраняемое состояние запуска, транзакционное вместе с командой, и показ экрана активного режима вместо экрана локации через единственный presentation source.
> **Затрагивает:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Commands and Events](../Architecture/CommandsAndEvents.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md), [Runtime Facade](../Architecture/RuntimeFacadeAndRegistries.md), [UI Document](../UI/UIDocumentAndReconciliation.md), [Presentation Authoring Reference](../Authoring/PresentationAuthoringReference.md).
> **Не является нормативным:** до реализации действует текущий contract.

## 1. Назначение

Текстовой RPG нужны механики, которые на время занимают игрока целиком: магазин, диалог, танец, бой, отчёт о конце хода. Сегодня их можно выразить только командами базовой локации и модальными окнами. Это не масштабируется: правила режима смешиваются с экраном локации, а промежуточный прогресс режима либо пишется в canonical state, либо живёт в произвольных `local`-таблицах модулей без изоляции, отката и тестов.

Предложение вводит один механизм в `core`, а семантику каждого режима оставляет пакету ([INV-016](../Architecture/Invariants.md), [ADR-0026](../ADR/0026-core-and-gameplay-ownership.md)). Цель — чтобы режим целиком писался на authoring surface, без `game.*`, сырого `game.state` и собственного presentation module.

Пакет делит режим на две части ([RH Gameplay Architecture § 5.12](../RH/GameplayArchitecture.md#512-временные-режимы)):

- **чистое ядро** — функции без состояния: получают значения участников и возвращают результат (оценка при продаже, расчёт танца, навигация по графу диалога, бой);
- **оболочка** — запуск, экран, команды режима и его временное состояние.

Механизм режимов нужен только оболочке; ядро от него не зависит и проверяется отдельно.

Предложение закрывает пробелы [GAP-001, GAP-002 и GAP-003](../Status/EngineGaps.md#временные-игровые-режимы). Первые потребители — режимы стартового пакета `rh` ([§ 3.1](#31-режимы-стартового-пакета-rh), [Engine Gaps § стартовый пакет](../Status/EngineGaps.md#стартовый-пакет-rh-что-блокирует)).

## 2. Состояние на входе

Проверено по коду и contracts на 2026-10-17.

- `Scripts/runtime/presentation_source.lua` допускает ровно один source; повторная регистрация даёт `PresentationSourceDuplicateRegistration`. [ADR-0039](../ADR/0039-presentation-source-singleton.md) сохранил singleton и назвал условие пересмотра: второй пакет, которому нужен собственный route, а не overlay или modal. Временный режим — именно такой потребитель.
- Единственный source регистрирует `textsystem:module.presentation.location_presenter` — обычный, не authoring module. В authoring `_ENV` нет способа объявить построитель экрана.
- `Scripts/runtime/command_dispatcher.lua` после каждой успешной команды вызывает `game.presentation.resolve()`, даже если команда не меняла canonical state. Отложенные команды (`:later`) dispatcher исполняет в том же вызове сразу после исходной, и `resolve()` вызывается после каждой из них.
- `show_screen`/`show_route` в `Scripts/authoring/presentation.lua` заменяют только `route`; `overlays` и `modals` остаются. [UI Document](../UI/UIDocumentAndReconciliation.md#routelayer-rules) требует «owner removal каскадно удаляет его modals», но у modal в Lua-документе нет owner, поэтому каскада нет.
- `AuthoringFailAfterMutation` отслеживает только canonical `write_revision`. Изменение `local`-таблицы до `fail()` не откатывается.
- `core:module.runtime.test_isolation` и `snapshot_test_state`/`restore_test_state` изолируют registries и canonical state, но не `local`-таблицы модулей.
- `game.random` хранит состояние потоков в canonical `state.meta.prng`.
- [Overview](../Architecture/Overview.md#категории-состояния) знает категорию «Lua runtime state — перестраивается при bootstrap/load». Прецедент такого состояния — `active_document_state` в `Scripts/authoring/presentation.lua`. Но [UI Document § UI-local state](../UI/UIDocumentAndReconciliation.md#ui-local-state) требует: значение, влияющее на gameplay, становится canonical state. Состояние незавершённого танца или боя влияет на gameplay и при этом намеренно не сохраняется — это новая категория, а не частный случай существующей.

## 3. Принятые решения

1. Location — режим по умолчанию и базовое представление мира.
2. Одновременно активен не более одного временного режима. Вложенные режимы не поддерживаются.
3. Состояние запуска режима (Mode Run) не сохраняется: после load или restart открывается Location по `location_id` игрока, незавершённый режим отбрасывается.
4. Команды режима — обычные `commands.*` с validators, typed refusal и action bindings. Отдельного диспетчера внутри режима нет.
5. Режим запускается из команды: из команды взаимодействия на локации, из отложенной команды реакции на событие или из другой команды пакета (конец хода, продажа).
6. Политика записи в canonical state задаётся режимом, а не механизмом (§ 3.1).
7. Сохранение во время режима правилом не запрещается: canonical state согласован, в save не попадает только незавершённый режим. UI режима не показывает кнопку сохранения.
8. UI-состояние (вкладки, модалки, выделения, scroll, номер показанной записи отчёта) не хранится ни в canonical state, ни в Mode Run, если оно не влияет на правила режима.
9. Mode Run хранит только то, что нужно оболочке между командами. Вычисления режима выполняет чистое ядро пакета.

### 3.1. Режимы стартового пакета `rh`

| Режим | Запуск | Mode Run | Запись в canonical state | Экран |
|---|---|---|---|---|
| Магазин | взаимодействие на локации | ID магазина | каждая покупка — сразу своей командой; запас бесконечный, цены фиксированные | [SR-003](../Status/ScreenRequests.md#sr-003-экран-магазина) |
| Диалог | взаимодействие с NPC, реакция на событие, продажа | ID диалога, текущий шаг, участники | последствия выбора — сразу; временной остаётся только позиция в графе | [SR-004](../Status/ScreenRequests.md#sr-004-экран-диалога) |
| Танец | взаимодействие с двумя и более NPC | участники, временные характеристики сцены, зерно потока случайности | итог участникам — одной командой в конце | [SR-005](../Status/ScreenRequests.md#sr-005-экран-танца) |
| Отчёт о конце хода | конец хода при непустом отчёте | записи отчёта | нет: результаты уже применены командой конца хода | [SR-002](../Status/ScreenRequests.md#sr-002-отчёт-о-конце-хода) |
| Бой (позже) | взаимодействие или реакция | значения участников, зерно | итог — одной командой в конце; время мира не сдвигается | заявка позже |

Продажа — не отдельный режим: команда продажи оценивает предметы игрока чистым ядром и запускает диалог исхода.

## 4. Модель состояния

| Категория | Владелец | Пишется | Сохраняется | Пример |
|---|---|---|---|---|
| Canonical gameplay-state | Lua, `game.state` | Command handler в mutation window | Да | золото, предметы, флаги мира |
| **Transient mode state** (новое) | `core`-механизм режимов | Command handler, через `modes` | Нет | открытый магазин, шаг диалога, временные характеристики танца, HP в текущем бою |
| Lua runtime state | Lua VM | Модули инфраструктуры | Нет | registries, UI document |
| Ephemeral UX state | UE | UE | Нет | hover, focus, scroll |

Transient mode state:

- хранится в одном `core`-хранилище, а не в `local`-таблицах пакетов: это даёт изоляцию в specs, чтение из validators и один путь отката;
- содержит только portable values (как command arguments): ID вместо wrappers, никаких functions, UObject или binding handles;
- меняется только внутри command handler — тот же принцип, что [INV-003](../Architecture/Invariants.md) для canonical state;
- **транзакционен вместе с командой**: механизм запоминает состояние запуска перед handler и восстанавливает его при `fail()` или ошибке. Снимается правило «проверить всё до изменения временного состояния», а `fail()` после изменения Mode Run, но до первой canonical записи остаётся корректным отказом;
- не входит в state hash и save; replay детерминирован, потому что тот же input даёт то же transient state.

Правило [UI-local state](../UI/UIDocumentAndReconciliation.md#ui-local-state) уточняется: значение, влияющее на gameplay, хранится в canonical state **или** в transient mode state активного режима.

## 5. Authoring-поверхность

Ниже — предлагаемая форма. Точные имена фиксируются ADR. Примеры следуют раскладке `rh` ([RH Gameplay Architecture § 2](../RH/GameplayArchitecture.md#2-устройство-пакета)): оболочка — в `modes/`, правила — Services в `rules/`, ID контента приходят из данных и аргументов.

### 5.1. Объявление режима

```lua
-- GameData/rh/scripts/authoring/modes/shop.lua
modes.shop = {
    -- Вызывается из команды старта; возвращает начальное transient state.
    start = function(args)
        return { shop = args.shop.id }
    end,

    -- Чистая проекция: canonical state + transient state -> screen spec.
    -- Не публикует, не мутирует.
    view = function(run)
        return {
            template = "textsystem:screen.shop",
            instance_key = "shop",
            fields = { ... },  -- из Definition магазина run.shop и денег игрока
        }
    end,
}
```

Короткий key даёт `<package>:mode.<key>` (новый Stable ID kind `mode`). Повторное объявление, late declaration и отсутствие `view` отклоняются на freeze, как для services.

### 5.2. Команды режима

```lua
local shop_rules = services["rh:service.shop"]

-- Запуск: эффект взаимодействия на локации или отдельная команда.
commands["shop.open"] = function(args)
    if modes.active() ~= nil then
        fail("mode.busy")                           -- проверка до первой записи
    end
    modes.shop:start({ shop = args.shop })
end

commands["shop.buy"] = function(args)
    local run = modes.shop:require_active()         -- fail("mode.not_active") иначе
    local reason = shop_rules.check_buy(player, run.shop, args.item)
    if reason then
        fail(reason.key, reason.params)
    end
    shop_rules.apply_buy(player, run.shop, args.item)   -- покупка сразу пишет canonical state
end

commands["shop.leave"] = function()
    modes.shop:require_active()
    modes.shop:finish({ outcome = "left" })
end
```

| Вызов | Где разрешён | Действие |
|---|---|---|
| `modes.<key>:start(args)` | command | Отказ `mode.already_active`, если активен любой режим; иначе создаёт Mode Run |
| `modes.<key>:require_active()` | command, validator | Возвращает run этого режима или `fail("mode.not_active")` |
| `modes.<key>:state()` | command, validator, event | Run или `nil`; в validator и event — read-only |
| `modes.active()` | везде | `mode_id` активного режима или `nil` |
| `modes.<key>:finish(outcome)` | command | Уничтожает run; `outcome` попадает в событие завершения |
| `modes.<key>:cancel()` | command | Уничтожает run без итога |

`modes.active()` читается в проверках пакета до первой canonical записи. Пакет, который проверяет всё до записи ([RH Gameplay Architecture § 2.4](../RH/GameplayArchitecture.md#24-проверка-отдельно-от-записи)), отказывает в запуске раньше, чем потрачены ресурсы. Отказ `start()` при активном режиме — защитный: после canonical записи он даёт `AuthoringFailAfterMutation`.

Побег из боя с последствиями — это `finish({ outcome = "escaped" })` плюс canonical изменения, а не `cancel()`.

Отказы атрибутируются по обычным правилам `fail()`: `mode.not_active` и `mode.already_active` принадлежат `core`.

### 5.3. События

Механизм публикует post-commit факты `core:event.mode.started` и `core:event.mode.finished` (`mode_id`, `outcome`; при `cancel()` — `outcome = "cancelled"`). Это позволяет модам и квестам реагировать на «танец завершён», не подписываясь на внутренние команды режима. Изменения transient state сами по себе событий не порождают.

## 6. Жизненный цикл

1. Команда старта проверяет предусловия и вызывает `modes.<key>:start(args)`. `location_id` не меняется. Команда старта может быть отложенной: подписчик на событие (например, переход) ставит в очередь команду реакции, а она проверяет условия, бросает шанс и запускает режим.
2. После ввода игрока и всех порождённых им отложенных команд `game.presentation.resolve()` строит экран активного режима (§ 7). Промежуточная интерактивная revision экрана Location между командой перехода и командой реакции не публикуется.
3. Команды режима меняют run и, если политика режима это предусматривает, canonical state.
4. Терминальная команда проверяет итог, применяет его к canonical state и вызывает `finish()`. При ошибке после первой canonical записи отката canonical state нет — его даёт [Mutation Window Transactionality](MutationWindowTransactionalityProposal.md); run при этом восстанавливается и остаётся активным.
5. После `finish()`/`cancel()` экран возвращается к Location.

Несколько реакций на одно событие конкурируют за запуск через правило одного активного режима: команда реакции проверяет `modes.active()` до записи и при активном режиме ничего не делает. Порядок реакций задаёт пакет.

Устаревшие нажатия UI отклоняются штатно по revision binding handles. `run_id` не вводится; для отложенных команд (`:later`) режим сам передаёт в аргументах нужный ему correlation value.

## 7. Презентация

### 7.1. Маршрутизация

Presentation source остаётся одним ([ADR-0039](../ADR/0039-presentation-source-singleton.md) не отменяется в части «один владелец route»). Меняется то, что `resolve()` вызывает:

```text
resolve():
    active = modes.active()
    if active == nil:
        document = default source()            -- location presenter, как сейчас
    else:
        document = view(active run) этого режима
```

Merge-проблема, из-за которой ADR-0039 отклонил несколько sources, не возникает: активен не более одного режима, и route принадлежит ровно одному владельцу в каждый момент. Default source регистрируется как сейчас; `textsystem` не зависит от `rh`, а router живёт в `core`.

Сейчас `resolve()` вызывается после каждой успешной команды, включая отложенные (§ 2). Получит ли UI промежуточную revision, зависит от того, когда coordinator забирает документ. Предлагается на это не полагаться: строить документ один раз после опустошения очереди отложенных команд, порождённых одним вводом.

### 7.2. Полная публикация документа и owner слоёв

- При `start()` route заменяется экраном режима, overlays и modals Location закрываются, всё — одной revision.
- `show_overlay`/`show_modal` внутри команды режима помечают слой owner = активный режим.
- При `finish()`/`cancel()` route возвращается к Location, слои с owner = режим удаляются той же revision. Это реализует уже требуемый contract каскад «owner removal удаляет его modals».
- Для автора вводится `replace_document({ route = ..., overlays = ..., modals = ... })` как общий helper одной revision; механизм режимов использует его же.

### 7.3. Экраны режимов

Механизм не зависит от конкретного template: `view` возвращает любой зарегистрированный Screen Template слоя route. Экраны режимов стартового пакета `rh` — отдельные templates `textsystem` в слое `location_content`, заявленные в [Screen Requests](../Status/ScreenRequests.md) (SR-002…SR-005). Их реализация — Widget Blueprint, schema и запись в Screen Registry — не входит в предложение. Демонстрационному режиму в тестовой фикстуре достаточно существующих templates.

## 8. Сохранение и загрузка

- Save пишет только canonical state; transient mode state не сериализуется.
- Load создаёт новую session; `modes.active()` равен `nil`, первым экраном становится Location.
- `core:command.session.save` во время режима не блокируется.

## 9. RNG в режимах

Решение принимается до первого режима, использующего случайность (танец или бой), рекомендуемая форма:

- команда старта режима один раз вызывает `game.random.next_u32(...)` из потока подсистемы пакета (`rh:random_stream.combat` и т. п.) — это обычная canonical запись в обычной команде;
- значение становится зерном потока внутри Mode Run (`modes.random(run, min, max)` или аналог на том же `xoshiro128ss-v1`), а чистое ядро получает поток аргументом;
- повторный танец или бой после load даёт другие броски, а внутри режима последовательность детерминирована.

Критерий «cancel оставляет state hash неизменным» для режимов со случайностью применяется ко всему state, кроме `meta.prng`.

## 10. Тестирование

- `test_isolation` получает изоляцию хранилища режимов (`with_isolated_modes` или включение в существующий snapshot/restore), поэтому активный режим не переходит из case в case.
- Spec может читать `modes.active()` и `modes.<key>:state()` для утверждений.
- Чистые ядра режимов проверяются specs пакета без механизма режимов; место specs пакета — [GAP-016](../Status/EngineGaps.md#gap-016-место-specs-пакета-игры).
- Демонстрационный режим для проверки механизма живёт в тестовой фикстуре, а не в production package ([ADR-0046](../ADR/0046-test-content-coupling-boundary.md)).

## 11. Решения, требующие явной фиксации

**ADR на новую категорию состояния.** Transient mode state — новая категория рядом с canonical state; она уточняет [UI-local state](../UI/UIDocumentAndReconciliation.md#ui-local-state) и Overview. Без ADR это противоречие contract.

**Пересмотр ADR-0039.** Условие пересмотра наступает. Предлагается не композиция sources, а routing между default source и view одного активного режима; это фиксируется в том же или отдельном ADR.

**Новый Stable ID kind `mode`.** Нужна запись в [Stable ID Specification](../Architecture/StableIDSpecification.md).

**Вызов `start()` при активном режиме.** Предлагается typed refusal `core:error.mode.already_active`, а не неявное завершение текущего режима.

**`emit` внутри команд режима.** Предлагается не ограничивать: команда, изменившая canonical state, публикует факты как обычно; команда, изменившая только run, может публиковать факты режима, если пакет считает их gameplay-фактами.

**Момент построения экрана при отложенных командах.** Построение после опустошения очереди (§ 7.1) меняет наблюдаемое поведение dispatcher и фиксируется в [Commands and Events](../Architecture/CommandsAndEvents.md).

## 12. Не входит в предложение

- Логика конкретных режимов: магазин, продажа, диалог, танец, бой, отчёт о ходе и их итоги — геймплей пакета ([RH Gameplay Architecture § 5.12](../RH/GameplayArchitecture.md#512-временные-режимы)).
- Собственные Screen Templates и Widget Blueprint режимов (§ 7.3).
- Сохранение и возобновление незавершённого режима.
- Вложенные режимы и одновременные режимы.
- Откат canonical state при ошибке — это [Mutation Window Transactionality](MutationWindowTransactionalityProposal.md).

## 13. Этапы

| Этап | Содержание | Где |
|---|---|---|
| M1 | Хранилище режимов, `modes` в authoring `_ENV`, транзакционность run вместе с командой, события, изоляция в specs | `Scripts/`, `Tests/Lua/` |
| M2 | Routing в `resolve()`, построение экрана после опустошения очереди отложенных команд, `replace_document`, owner слоёв и каскад при выходе | `Scripts/`, `Tests/Lua/presentation/` |
| M3 | Демонстрационный режим в тестовой фикстуре: start, команды run, cancel, finish, запуск отложенной командой, save/load | `Tests/` |
| M4 | Магазин, диалог (с выводом продажи и реакций на события) и отчёт о конце хода в `rh` | `GameData/rh/` |
| M5 | RNG режима, затем танец и бой | `Scripts/`, `GameData/rh/` |

M1–M3 — работа по движку; M4–M5 — геймплей на authoring surface. M4 дополнительно требует экранов SR-002…SR-004, места specs пакета ([GAP-016](../Status/EngineGaps.md#gap-016-место-specs-пакета-игры)) и нового `rh` вместо заглушки ([GAP-017](../Status/EngineGaps.md#gap-017-тесты-движка-опираются-на-заглушку-rh)); танец — экрана SR-005.

## 14. Трудоёмкость

**M.** Новый механизм в `core`, правки dispatcher (снимок и восстановление run, момент построения экрана), presentation routing и authoring helpers; основная работа — specs на обоих host-ах и регрессия существующих presentation specs.

## 15. Критерии приёмки

- Режим объявляется и работает целиком из authoring module, без `game.*` и сырого `game.state`.
- Зарегистрирован ровно один presentation source; `PresentationSourceDuplicateRegistration` не возникает.
- Без активного режима presentation specs Location проходят без изменений: сегодня `location_transition_spec`, `declarative_location_spec`, `rh_layered_presentation_spec`, `dynamic_menu`, `inventory_tabs_spec`; после [GAP-017](../Status/EngineGaps.md#gap-017-тесты-движка-опираются-на-заглушку-rh) — их версии на фикстурах.
- `start()` при активном режиме отклоняется до побочных эффектов; `modes.active()` доступен в проверках до первой canonical записи.
- Режим, запущенный отложенной командой в ответ на событие, показывается без промежуточной интерактивной revision экрана Location.
- Команда, изменившая только run, не меняет state hash.
- `fail()` или ошибка в команде режима оставляют run равным состоянию до команды.
- `cancel()` оставляет state hash равным хешу до старта режима (для режимов со случайностью — кроме `meta.prng`).
- `finish()` применяет итог один раз; повторная команда режима после `finish()` отклоняется `mode.not_active`.
- После выхода из режима UI document не содержит overlays и modals с owner = режим; вход и выход публикуют по одной revision.
- Save во время режима не содержит Mode Run; после load открыта Location и `modes.active()` равен `nil`.
- Активный режим не переходит между spec case-ами.
- Одинаковая последовательность input даёт одинаковый canonical hash в headless и UE.
