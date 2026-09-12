---
title: Cpp Foundation Session Lifecycle
status: active
version: 1.0
updated: 2026-09-12
depends_on:
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../UI/UIDocumentAndReconciliation.md
---

# Владение session и её публикация

> **Материализует:** M1 [плана](README.md): удаление второго schema authority, проверяемое sealing и один production protocol replacement. Зависит от CFC-01…03.

## CFC-04 — Сделать snapshot единственным источником UI-схем

- [ ] CFC-04 — Сделать snapshot единственным источником UI-схем

**Файлы:** изменить `Source/GV2/Private/Application/GV2SessionContentSnapshot.h/.cpp`, `GV2ScreenFieldMaterializer.h/.cpp`, `GV2SessionCoordinator.cpp` в том же каталоге; `Source/GV2/Private/UI/GV2UiSchemaCache.h/.cpp`; `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`; `Docs/UI/ScreenTemplates.md`, `Docs/Guides/AddScreenField.md`. Actual additional callers определить поиском declarations/references materializer и schema cache, не ограничивать этими стартовыми файлами.

**Интерфейс:** существующий `FGV2PresentationPrepareContext` передаётся обязательной const reference каждому schema-consuming entry point materializer. Контекст содержит snapshot соответствующей generation; nullable/default overload запрещён. Cache creation/CompileAll доступны только builder-у snapshot; lookup read-only.

**Инвариант:** [Bootstrap Cold start](../../Architecture/BootstrapAndSessionLifecycle.md#cold-start), [Screen Templates](../../UI/ScreenTemplates.md). Наличие schema cache в snapshot недостаточно: production consumer обязан читать именно его.

**Не считается закрытием:** перестроить global cache из тех же roots; предпочесть snapshot при наличии, оставив fallback; тестировать два cache constructor без настоящего materializer.

**Шаги:**
1. Создать fixture A с различимыми top-level, binding и nested schema constraints. Построить snapshot A; изменить source files и построить B с иными constraints. Подготовить реальные documents через materializer A и B, проверить разные заранее заданные outcomes. На исходном global path тест обязан выявить неправильный источник.
2. Удалить `GSessionSchemaCache`, `RebuildSchemaCacheForSession`, default schema lookup и самостоятельное discovery ниже builder. Протащить обязательный context через каждый compile error/caller; у binding/nested recursion использовать тот же context.
3. Сузить constructors cache/builder так, чтобы runtime lookup нельзя было превратить в новое discovery. Source gate выводит actual constructors/lookup callers из исходников и запрещает создание вне snapshot builder; неизвестная форма вызова не пропускается. Gate подключить в CTest, production success/failure fixtures — в UE Automation.
4. Обновить контракт и Guide тем же change set, выполнить negative mutation с возвратом global path, полные относящиеся к materializer tests и документационный validator.

**Done:**
- Все фактические schema lookup callers требуют context; перечислитель — declarations/references actual API, полнота вызовов дополнительно обеспечивается удалением прежних signatures и компилятором.
- Cache construction/discovery из runtime materializer запрещены API/gate.
- Top-level, binding и nested production сценарии используют заранее ожидаемые schemas A/B после изменения файлов.
- Повторная обработка A не читает filesystem; счётчик установлен на реальном discovery adapter, не на test-only proxy рядом с ним.
- При полном закрытии удалён STATUS-013, PSC-AF-03 дополнен исходом CFC-04.

**Evidence:** A/B inputs и outcomes, discovery count, caller inventory, compile/gate negative result, UBT/UE acceptance и docs validation. Совпадающий pointer без отличающегося schema behavior — недостаточное доказательство.

## CFC-05 — Сделать registry sealing обязательной фазой запуска

- [ ] CFC-05 — Сделать registry sealing обязательной фазой запуска

**Файлы:** изменить `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h`, `Scripts/bootstrap/main.lua`, `Scripts/bootstrap/manifest.lua`, `Scripts/runtime/state_validator.lua`; actual registry providers найти по их `game` publication и `freeze/is_frozen` definitions. Создать `Scripts/bootstrap/registry_lifecycle.lua`, `Tests/Lua/lifecycle/registry_sealing.lua`, `Tools/Testing/validate_registry_lifecycle_ownership.py`; native mechanism conformance и его public entry point добавить по [канонической форме](../../Architecture/BuildAndTooling.md#каноническая-форма-conformance-entry-point), подключить в обоих hosts. Документы: `RuntimeFacadeAndRegistries.md`, `LuaRuntimeContract.md`, `Docs/Guides/AddLuaModule.md`.

**Интерфейс:** private `registry_lifecycle.install()` до module `register` hooks создаёт/устанавливает engine registries из одного descriptor; `registry_lifecycle.seal()` после hooks разрешает ссылки, исполняет freeze по contract order и требует `is_frozen() == true`. Оба вызова известны bootstrap, функции не экспортируются в authoring/gameplay. Registry-bearing façade slots доступны module hooks только через read-only façade: заменить registry object или добавить ad hoc slot нельзя; registration методов самих registries это не запрещает. Privileged installation handle остаётся private Lua bootstrap, C++ не хранит его. C++ вызывает fixed `seal` через общий protected-call механизм; результат — success либо существующий `FRuntimeFault` с registry path и стадией. Descriptor не содержит gameplay entries и не расширяется mods.

**Инвариант:** [Host-side freeze sequence](../../Architecture/RuntimeFacadeAndRegistries.md#host-side-freeze-sequence). State build возможен только после полного sealing. Перечислитель — тот же descriptor, который фактически создаёт/устанавливает registries; список только для теста запрещён. Ожидаемый порядок и семантика fault задаются независимым contract fixture.

**Не считается закрытием:** проверить один `lua_pcall`; пропустить отсутствующий registry; добавить новую рукописную последовательность C++ вызовов; объявить private descriptor generic extension registry.

**Шаги:**
1. Перенести воспроизведение RUNTIME-AF-01 в native mechanism test через реальный `FRuntimeSession::Start`: throwing freeze; фиксировать `Start == false`, fault и отсутствие вызовов state/start hooks.
2. Свести создание и sealing engine registries в private bootstrap composition. Существующие providers сохраняют свои implementations и policies; убрать их самостоятельное размещение в `game` и C++ path-by-path обход. Reference-field/schema sealing включить в ту же завершённую фазу; module export freezing остаётся loader-owned.
3. Перечисляя actual descriptor, по одному подменять participant: missing object, missing method, throw, ложный/неверного типа frozen result. В каждом случае state build не вызывается. Positive fixture проверяет contract order и позднюю регистрацию после freeze.
4. Read-only façade запрещает production module hook заменить registry object/ввести ad hoc registry slot в рантайме; проверить реальной попыткой из пакета. Второй рубеж — gate actual registry publications из Lua source syntax: замкнуть поддержанную форму publication; dynamic assignment, неизвестный publisher или незарегистрированный registry factory делают gate красным. Проверить negative mutation нового registry, добавленного в обход descriptor. Текстовый gate без runtime запрета публикации не является достаточной структурной защитой.
5. Проверить native stack/context restoration и запрет последующего hook после fault в обоих hosts; Lua semantics проверять Lua specs. Синхронизировать contracts/Guide, выполнить shared conformance, `--check-scripts`, UE acceptance, docs validation.

**Done:**
- Для каждого фактически установленного registry failure до state build воспроизводится через public session start.
- Нельзя опубликовать новый production registry вне enumerated composition незаметно для gate.
- `freeze_reference_fields` и authoring finalization errors проходят тот же fail-closed gate.
- Protected-call failure восстанавливает native stack/context; незамороженная session не публикуется.
- Registry functions/callbacks остаются внутри Lua; C++ получает только outcome.
- STATUS-017 удалён после conformance обоих hosts; RUNTIME-AF-01 получает исход CFC-05.

**Evidence:** actual participant inventory, independent expected order, fault matrix, hook trace без state/start после failure, bypass mutation, portable/UE результаты. Тест только `registry.freeze()` без `FRuntimeSession::Start` не закрывает задачу.

## CFC-06 — Передавать candidate явно и объединить publication с UE projection

- [ ] CFC-06 — Передавать candidate явно и объединить publication с UE projection

**Файлы:** `Source/GV2/Private/Application/GV2SessionCoordinator.h/.cpp`, `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, `Source/GV2/Public/Runtime/GV2RuntimeSubsystem.h`, `Source/GV2/Public/Bridge/GV2BridgeTypes.h`, production reconciler/factory callers из actual references; `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`, `GV2RuntimeSubsystemTests.cpp`; `Docs/Architecture/BootstrapAndSessionLifecycle.md`, `Docs/UI/UIDocumentAndReconciliation.md`, `Docs/UI/ScreenTemplates.md`.

**Интерфейс и ownership:**
```cpp
using FDocumentSink = TFunction<bool(
    const FGV2UiDocumentViewModel&,
    const FGV2PresentationPrepareContext&)>;
```
Context приходит из конкретной active/candidate generation, не запрашивается обратно через coordinator getter. `GetContentSnapshot()` возвращает snapshot только текущей Ready-session. У coordinator private move-only replacement token, владеющий native candidate и стадией `Preflight`, `Replacing`, `Preparing`, `Committed` или `Aborted`; host не может его создать. Token не является Lua-visible DTO. Единственная owner routine проводит `BeginReplace` и `PublishReady`; все host projection mutations получают разрешение из этой routine, без независимого пути в `StartSession` adapter.

**Инвариант:** [New/load session build](../../Architecture/BootstrapAndSessionLifecycle.md#newload-session-build) и [Apply boundary](../../UI/UIDocumentAndReconciliation.md). Initial document B всегда готовится из B; Ready/snapshot/UI относятся к одной generation.

**Не считается закрытием:** поменять порядок веток getter; запустить только A и отдельно построить B; сохранить только coordinator status при уже удалённом shell; оставить global GameShell settings lookup.

**Шаги:**
1. Исполнить два успешных `StartSession` на одном production coordinator: A и B с разными screen/schema/theme/resource значениями. Внутри второго document sink проверить B. Fixture B обязан реально доходить до initial Commit; снять прежнее ограничение теста, из-за которого он строил только candidate.
2. На верхнем `UGV2RuntimeSubsystem` запустить A, затем инициировать candidate failure B. Проверить прежние UObject instances, viewport attachment, geometry/fields, bindings и успешную команду через прежний UI A. Это red scenario раннего teardown.
3. Удалить `GetContentSnapshotForPrepare()` и `InProgressCandidate` как ambient lookup. Протащить explicit context через document sink/reconciler/screen factory; shell создавать из `Context.Snapshot` и до Ready не публиковать как active projection.
4. Перенести host teardown под единый coordinator replacement token: content build/preflight не трогают A; `BeginReplace` закрывает input и уничтожает A; `PublishReady` завершает B после initial apply. Private APIs/type-state запрещают повторное использование token и публикацию без prepared result.
5. Gate выводит actual assignments active snapshot/status и host projection teardown/publication callers из source; writes за пределами owner routines запрещены. Conditional bypass/новый direct caller в negative fixture обязан ломать gate. Reentrant observer получает согласованный published state либо not-ready; между частями commit внешние callbacks не исполняются.
6. Проверить реальные failure initial Prepare/Apply B после teardown: A не воскрешается, B не выдаётся за Ready, отображается native recovery. Обновить contracts и прогнать full UE acceptance.

**Done:**
- A→B с реальными двумя стартами использует B во всех initial authority resolutions.
- До BeginReplace failure сохраняет исполнимый UI A, VM/generation/snapshot/bindings A.
- После BeginReplace failure оставляет not-ready/recovery без ложной Ready A/B.
- GameShell identity берётся из того же snapshot, что screen/theme/resources.
- Все production publication/teardown sites выведены из исходников и принадлежат одному owner protocol; новая точка записи ломает gate.
- STATUS-014/015 и PSC-AF-04/05 закрываются только после верхнего UE сценария, а не одного coordinator test.

**Evidence:** traces двух commit points, identity/field assertions A/B, успешный interaction A после preflight failure, recovery B, source-site inventory и mutation diagnostics, UBT/full UE. Пользовательские viewport/button изменения сохранить и проверить в общей ветви без присвоения их авторства.

## CFC-07 — Завершить lifecycle requests, отмену и teardown

- [ ] CFC-07 — Завершить lifecycle requests, отмену и teardown

**Файлы:** coordinator/subsystem/bridge types из CFC-06; `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` и public header; создать `Source/GV2/Private/Application/GV2SessionTransition.h/.cpp` для private transition policy в существующем GV2 module и `Source/GV2/Private/Tests/GV2SessionTransitionTests.cpp`; portable phase-result mechanism tests/shared host adapters; `Docs/Architecture/BootstrapAndSessionLifecycle.md`, `Docs/Architecture/LuaRuntimeContract.md` (operation/re-entry boundary).

**Интерфейс:** typed `FSessionStartDescriptor` содержит `Mode` (`Menu`, `NewGame`, `LoadSave`), slot ID для load и exact repository identity. `RequestSession(Descriptor)` возвращает operation ID; `CancelSessionRequest(OperationId)` возвращает typed accepted/too-late/stale. Operation outcomes: completed, failed, cancelled, superseded; никаких Lua callback names. Существующие Start/End adapter методы делегируют единственной transition policy. Согласованные типы размещаются в bridge types, private transition policy их исполняет; CFC-10 подключит проверенные load bytes.

**Инвариант:** [Lifecycle requests / teardown](../../Architecture/BootstrapAndSessionLifecycle.md). Не более одной VM; одна активная transition, один pending last-wins request, shutdown priority. Это native orchestration существующих hooks, gameplay rule в C++ не появляется.

**Не считается закрытием:** задним числом выставить пропущенные states после синхронного `Start`; тестировать модель states, которую runtime не вызывает; реализовать только cold NewGame и удалить STATUS-001.

**Шаги:**
1. На production entry points записать failure traces отсутствующих restart/menu/cancel переходов. Завести closed phase enum на реальных границах `Registering`, `BuildingState`, `RestoringInstances`, `Starting`, `PreparingPresentation` и readiness, используя существующие state types; переходы идут через один проверяющий routine.
2. Дать native orchestrator наблюдать завершение реальной фазы runtime. Cancellation обрабатывается между фазами после protected call, не прерывает Lua; reentrant request только ставит очередь. NewGame/LoadSave входят в один state builder с разным typed input, а не в два lifecycle-клона.
3. Реализовать join эквивалентных requests, supersede pending slot, stale generation rejection и shutdown priority. Замкнутое множество request kinds берётся из enum; exhaustive switch без default и coverage enum обеспечивают появление нового вида в проверке.
4. Подключить module stop/unregister в reverse order, обязательный cleanup после первого user-hook error, cancellation операций, очистку ingress/Bridge и destruction VM. Hooks получают существующий Lua module graph load order; второй ручной список модулей не вводится.
5. Для каждого actual phase transition выполнить cancellation/failure injection из production runner. Oracle — независимая таблица разрешённых transitions из contract fixture; actual trace — production transition routine. Gate запрещает прямые state assignments/VM creation в обход неё; сам helper без вызывающего не считается проверкой.
6. Исполнить Menu→Game→Menu, Restart, content reload/restart, shutdown и stale interaction/completion. Menu создаётся существующим package-owned Lua start/presentation, не C++ synthetic gameplay-state. Load preflight/replace проверяется CFC-10; до этого STATUS-001 сохраняется с точным оставшимся gap.

**Done:**
- Реальные lifecycle phases видны при их исполнении; cancellation проверяет каждую actual разрешённую границу до Ready.
- Максимум одновременно живых VM равен одному по счётчику в общем production VM creation/destruction owner.
- Join/last-wins/shutdown и terminal operation results подтверждены public request сценариями.
- Hook error останавливает следующие user hooks, но не native cleanup; stale input/completion не меняет новую session.
- Native projection teardown соблюдает две границы CFC-06 при cancellation до/после BeginReplace.
- STATUS-001 остаётся открытым до полного load-another-save сценария CFC-10.

**Evidence:** phase/request enum inventory, independent transition matrix, VM count, hooks/operation traces, source bypass mutations, shared conformance и UE tests. Рукописная таблица oracle не называется enumerator actual execution.
