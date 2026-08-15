---
title: Migration and Gate Tasks
status: archived
version: 1.4
updated: 2026-08-15
depends_on:
  - LuaSpecRunner.md
decisions:
  - ../../../ADR/0024-lua-spec-runner.md
---

# M4 — Migration and Gate

## Результат этапа

Runner доказан на реальных наборах, а возврат к C++-проверке Lua-правил обнаруживается автоматически.

## Задачи

- [x] **TAS-12 — Мигрировать наборы GEW-04 и GEW-05**
  - `GV2WorldDomainObjectConformance` (439 строк) и `GV2WorldCurrentLocationConformance` (575 строк) переносятся в спеки.
  - Перенос не меняет содержание проверок: те же кейсы, те же условия, то же покрытие.
  - Done: оба C++-файла удалены; спеки исполняются обоими host-ами; идентификаторы кейсов сохранены либо их изменение зафиксировано явно; объём удалённого C++ указан в change set.
  - Evidence:
    - **Инфраструктурная предпосылка, вскрытая этим переносом**: спеки исполняются на *продакшн*-сессии (реальный `Scripts/bootstrap/manifest.lua`, реальный `game.repository`) — в отличие от старых C++-наборов, каждый из которых строил свою изолированную минимальную in-memory сессию. Продакшн-сессия по умолчанию держит `mutation_window` закрытым, поэтому кейс, желающий мутировать `game.state`, обязан открыть окно явно через `require("core:module.runtime.mutation_window").execute_in_window(fn)` (публичная функция, уже использовавшаяся `command_dispatcher`). Чтобы такая мутация не утекала в digest (`RunResult.StateHash` в `gv2-headless` вычисляется из того же `game.state`, что и любая другая мутация), `Headless/Source/main.cpp` теперь стартует **отдельную** `SpecSession` только для `GV2TestSupport::RunLuaSpecs`, а не переиспользует `Runtime` — до этой правки TAS-04 использовал общую сессию, что было безопасно только пока ни один мигрированный набор не писал в state.
    - **`Tests/Lua/world/domain_object.lua`** (GEW-04, 4 кейса: `world_is_callable`, `repeated_access_returns_distinct_wrappers`, `wrapper_writes_delegate_to_state_world`, `wrapper_rejected_when_stored_in_state`) и **`Tests/Lua/world/current_location.lua`** (GEW-05, 4 кейса: `valid_location_reference_accepted`, `wrong_kind_reference_rejected`, `dangling_reference_rejected`, `current_location_readable_without_mutation`) — те же условия, что в удалённых C++-наборах, дословно перенесённые проверки. Проверка kind/dangling для GEW-05 теперь идёт против **реальных** definitions продакшн-репозитория (`core:location.city.market`, `core:screen.main`), а не синтетического in-memory fixture, которым пользовался старый C++-набор — сам факт проверки (grammar → kind → repository existence) идентичен.
    - **Идентификаторы кейсов изменены явно, в лучшую сторону**: старые C++-наборы не давали per-case identifiers — единственный failure path у обоих был `Session.Start()` со свёрнутым в один blob `LuaModuleLifecycleError`, различить конкретный проваленный `assert` внутри монолитного Lua-driver-а можно было только по тексту сообщения. Новые спеки дают точный `<spec>.<case>` (например `world.domain_object.repeated_access_returns_distinct_wrappers`) по контракту TAS-03 — строго более гранулярно.
    - Оба C++-файла (`GV2WorldDomainObjectConformance.h/.cpp`, `GV2WorldCurrentLocationConformance.h/.cpp`, суммарно 1076 строк) удалены; вся обвязка вычищена — `Source/CMakeLists.txt`, `Headless/Source/main.cpp` (includes + вызовы, exit codes 13/14 освобождены), `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp` (includes + два `IMPLEMENT_SIMPLE_AUTOMATION_TEST`).
    - Проверено вживую перед удалением C++: временный сбой (`w1 == w2` вместо `~=`) в `domain_object.lua` корректно провалил `gv2-headless --self-test` (exit 16, `lua_spec_failed id=world.domain_object.repeated_access_returns_distinct_wrappers code=LuaSpecCaseFailed`) и `GV2.Runtime.Lua.SpecRunnerHost` (`AddError` с тем же id); аналогично для `current_location.lua`. Оба сбоя откачены перед удалением C++.
    - Оба хоста после удаления C++: `gv2-headless --self-test` exit 0, digest не изменился (`20dc1392...`); UE `GV2.Runtime` 52/52 (было 54 — минус два отдельных per-spec теста, покрытых теперь одним `SpecRunnerHost`). `ctest` 57/57. `validate_host_conformance_parity.py`: 28 → 26 entry points (ожидаемо — два удалённых C++ entry point-а).

- [x] **TAS-13 — Мигрировать наборы command validators**
  - `GV2CommandValidatorInvocationConformance` и `GV2CommandRefusalSemanticsConformance` переносятся в спеки.
  - Done: покрытие не изменилось; порядок валидаторов и семантика отказа проверяются спеками; C++-файлы удалены.
  - Evidence:
    - **Архитектурное ограничение, не встречавшееся в TAS-12**: реестр валидаторов (`game.commands.validators`) реальной продакшн-сессии уже заморожен и пуст к моменту исполнения любой спеки (ни один продакшн-модуль пока не регистрирует ни одного валидатора). GEW-02/03 по своей сути тестируют взаимодействие command dispatcher-а с тест-scoped валидаторами/командами, зарегистрированными во время фазы `register`, до заморозки — специфика, которую продакшн-сессия предоставить не может. Решение: спеки под `Tests/Lua/commands/` исполняются на отдельной, изолированной fixture-сессии, а не на продакшн-сессии.
    - **`GV2TestSupport::StartCommandValidatorFixtureSession()`** (новый, `Source/GV2TestSupport/Public/GV2TestSupport/CommandValidatorFixture.h`, `.../Private/CommandValidatorFixture.cpp`) — единственная реализация, общая для обоих host-ов: читает с диска РЕАЛЬНЫЕ `Scripts/runtime/{mutation_window,stable_id,validator_registry,command_dispatcher}.lua` (не дублирует их embedded C++-строками, в отличие от удалённых старых наборов) плюс test-only fixture `Tests/Fixtures/CommandValidatorSpecs/{manifest,driver}.lua` (новый; вне `Scripts/` — никогда не грузится реальным module loader-ом и не стейджится в игру; вне `Tests/Lua/` — не подхватывается `DiscoverLuaSpecFiles` как спека). `driver.lua` — прямой перенос обоих старых C++-driver-ов (объединены: id-коллизий между тест-командами/валидаторами GEW-02 и GEW-03 нет), теперь как обычный `.lua`-файл вместо embedded string literal.
    - **`Tests/Lua/commands/validator_invocation.lua`** (GEW-02, 4 кейса) и **`Tests/Lua/commands/refusal_semantics.lua`** (GEW-03, 4 кейса) — те же условия, что в удалённых C++-наборах. Диспетчеризация теперь идёт прямым вызовом `game.runtime.dispatch_command(...)` из Lua (спека исполняется внутри VM — C++-boundary `FRuntimeSession::DispatchCommand()` не нужен); provider/dispatcher fault, который старый C++-тест видел как `FRuntimeFault{"LuaDispatchError", ...}`, теперь виден как обычная Lua-ошибка, пойманная `pcall` — эквивалентная, не урезанная проверка.
    - **Разделение `Tests/Lua/` по под-деревьям**: обнаружено, что один recursive-скан всего `Tests/Lua/` с одной сессией (как было после TAS-12) не работает — `Tests/Lua/commands/*.lua` не могут делить сессию с `Tests/Lua/world/*.lua`. `Headless/Source/main.cpp` теперь делает два отдельных вызова `RunLuaSpecs`: один на `Tests/Lua/world` с production-сессией (TAS-12), второй на `Tests/Lua/commands` с fixture-сессией. Аналогично на UE: `GV2.Runtime.Lua.SpecRunnerHost` сужен до `Tests/Lua/world`, добавлен отдельный `GV2.Runtime.Lua.CommandValidatorSpecRunnerHost` для `Tests/Lua/commands` (по-прежнему один тест на всё под-дерево, не на спеку — контракт TAS-04 не нарушен).
    - Проверено вживую (временные сбои в обеих спеках на обоих host-ах, корректные `id`/сообщения, затем откачены): `gv2-headless --self-test`, UE `CommandValidatorSpecRunnerHost`.
    - Оба C++-файла (`GV2CommandValidatorInvocationConformance.h/.cpp`, `GV2CommandRefusalSemanticsConformance.h/.cpp`, суммарно 1299 строк) удалены; обвязка вычищена — `Source/CMakeLists.txt`, `Headless/Source/main.cpp`, `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`.
    - `ctest` 57/57, UE `GV2.Runtime` 51/51 (было 54 до TAS-12 → 52 после TAS-12 → 53 после добавления нового теста → 51 после удаления двух старых). Digest не изменился. `validate_host_conformance_parity.py`: 26 → 24 entry points (ожидаемо — два удалённых C++ entry point-а).

- [x] **TAS-14 — Расширить гейт паритета**
  - `Tools/Content/validate_host_conformance_parity.py` обязан обнаруживать новый C++ conformance entry point, проверяющий Lua-правило.
  - Отличать его от легитимного C++ entry point, проверяющего C++ API, по явному признаку — расположению, соглашению об имени или объявленной категории.
  - Done: попытка добавить C++-проверку Lua-правила ломает CI; существующие немигрированные наборы не ломают CI и помечены как унаследованные; ложное срабатывание на C++ API отсутствует.
  - Evidence: Новая функция `validate_no_new_lua_rule_conformance()` сканирует все `*Conformance.cpp` под `Source/` (переиспользует уже существующий детектор embedded-Lua raw/inline строк из `validate_no_embedded_lua_in_production()`) и требует явной категоризации по относительному пути от любого файла, встраивающего Lua:
    - `LEGACY_LUA_RULE_CONFORMANCE_FILES` — унаследованные до ADR-0024 наборы (`GV2LuaLifecycleConformance.cpp`, `GV2LuaRepositoryConformance.cpp`, `GV2ValidatorRegistryConformance.cpp`) — явно помечены как унаследованные, список не расширяется.
    - `MECHANISM_LUA_FIXTURE_CONFORMANCE_FILES` — тестируют C++-механизм, не Lua-правило: `GV2LuaSpecRunnerConformance.cpp` (сам spec runner, TAS-02) и обнаруженный по ходу задачи `GV2RunReplayConformance.cpp` (`ReplayRunManifest()` — синтетический `dispatch_command`-стаб без прообраза в `Scripts/`, определяющий и заваливающий детерминизм/fault-механику replay, а не gameplay-правило).
    - Файл вне обоих списков со встроенным Lua — ошибка с текстом, объясняющим, какой список пополнить и почему (обеспечивает «явный признак» из Done).
    - Проверено вживую: временно создан `GV2DummyRuleConformance.cpp` с синтетическим Lua-модулем — gate корректно провалился с указанием на новый файл; файл удалён, gate снова зелёный.
    - `ctest` 57/57 (включая `host_conformance_parity_contract`), `validate_host_conformance_parity.py` — 24 entry points, ложных срабатываний на существующих mechanism/legacy файлах нет.

- [x] **TAS-15 — Синхронизировать документацию и снять паузу**
  - Зависимости: TAS-12–TAS-14.
  - Done: `HeadlessSimulationContract` и `BuildAndTooling` описывают фактическое положение дел, включая перечень немигрированных наборов; `ImplementationStatus` обновлён; в `GameplayEventsAndWorld` снята пометка о приостановке, а его оставшиеся этапы переведены на спеки.
  - Evidence:
    - `HeadlessSimulationContract` раздел «Conformance» переписан: две допустимые формы проверки с таблицей границы по предмету, запрет нового C++ entry point на Lua-правило, закрытый список унаследованных наборов. Добавлен подраздел «Сессии спек»: `Tests/Lua/world/` на продакшн-сессии с явным открытием mutation window, `Tests/Lua/commands/` на изолированной fixture-сессии, спеки исполняются на сессии, отдельной от производящей run digest.
    - `BuildAndTooling` получил раздел «Lua-спеки» перед разделом о C++ entry point: формат спеки, идентичность провала `<spec>.<case>`, отсортированное рекурсивное обнаружение, привязка под-дерева к сессии, точки запуска и exit code 16. Раздел о C++ entry point явно ограничен проверкой C++ API. В описание CI добавлен перечень унаследованных наборов с объёмом (`GV2LuaLifecycleConformance` 5326, `GV2LuaRepositoryConformance` 489, `GV2ValidatorRegistryConformance` 332) и правило, что список закрыт.
    - `ImplementationStatus`: строка parity gate уточнена до 24 entry points плюс спеки и трёх проверок гейта; добавлены строки «Миграция conformance на спеки» (мигрировано world и command validators, удалено 2375 строк C++) и «Независимость тестов от контента».
    - `GameplayEventsAndWorld`: пометка о приостановке снята, «Состояние на входе» переписано под фактическое (M1/M2 выполнены), в принятые решения добавлено «Проверки пишутся Lua-спеками», в правила выполнения — правило выбора сессии для спеки. Оставшиеся этапы M3–M5 переведены на спеки: `EventBusCore` объявляет под-дерево `Tests/Lua/events/` на fixture-сессии, `SubscriptionAndReaction` и `TravelSlice` ссылаются на спеки вместо conformance-наборов, `GEW-15` уточнён под TAS-08 (golden строится на замороженном корпусе, добавление контента в `GameData/core` его не трогает).
    - `Docs/Plans/README.md`: снята пометка о приостановке, координационная заметка переписана — добавление контента в `GameData/core` больше не меняет ни одного pinned-значения.
    - Проверено перед правкой документации: сборка без предупреждений, `ctest` 57/57, `validate_host_conformance_parity.py` — 24 entry points, zero embedded Lua in production C++. `validate_docs.py` — 98 файлов.

## Проверка milestone

- [x] Наборы GEW-04/GEW-05 и validators живут спеками, C++-файлы удалены.
- [x] Попытка проверить Lua-правило из C++ ломает CI.
- [x] Унаследованные наборы не ломают CI и перечислены явно.
- [x] Пауза `GameplayEventsAndWorld` снята.
