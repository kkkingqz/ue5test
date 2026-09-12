---
title: Cpp Foundation Session Lifecycle
status: active
version: 1.2
updated: 2026-09-12
depends_on:
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../UI/UIDocumentAndReconciliation.md
---

# Владение session и её публикация

> **Материализует:** M1 [плана](README.md): удаление второго schema authority, независимое владение resolved registry, проверяемое sealing и один production protocol replacement. Зависит от CFC-01…03.

## CFC-04 — Сделать snapshot единственным источником UI-схем

- [x] CFC-04 — Сделать snapshot единственным источником UI-схем

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

## CFC-04A — Сделать resolved Screen Registry независимым значением snapshot

- [ ] CFC-04A — Сделать resolved Screen Registry независимым значением snapshot

**Зависимость:** CFC-04; задача предшествует CFC-05/06. Закрывает SNAP-AF-01 (внешнее имя SNAP-R1), STATUS-020. Исполняется до проверки сохранности A при replacement: сохранение ссылки на A бессмысленно, если candidate B уже изменил её содержимое.

**Файлы:** `Source/GV2/Private/Application/GV2SessionContentSnapshot.h/.cpp`, `Source/GV2/Public/UI/GV2ScreenRegistry.h`, `Source/GV2/Private/UI/GV2ScreenRegistry.cpp`, `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`, `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`; callers и tests `UGV2ScreenRegistry::Build/Resolve/GetResolvedScreenIdentities` определить по actual references. Обновить `Tools/Testing/validate_screen_registry_entry_encapsulation.py`; создать `Tools/Testing/validate_session_snapshot_ownership.py` с negative self-tests и подключить в `Source/CMakeLists.txt`. Docs: `Docs/Architecture/BootstrapAndSessionLifecycle.md`, `Docs/UI/ScreenTemplates.md`, `Docs/Architecture/BuildAndTooling.md`, `Docs/Authoring/AddUIScreen.md` (authoring DataAsset и runtime resolution).

**Инвариант:** [ADR-0043 D1](../../ADR/0043-presentation-apply-boundary.md), [session candidate](../../Architecture/BootstrapAndSessionLifecycle.md). Published A и candidate B не разделяют mutable registry runtime state. `TStrongObjectPtr` защищает lifetime объекта, но не даёт изоляцию или immutable contents. Это выполнение уже принятого ownership, отдельного изменения инварианта не требуется.

**Интерфейс и ownership:** `UGV2ScreenRegistry` сохраняет authoring `Entries` и compile operation с const input. Рекомендуемая сигнатура — `bool CompileResolvedRegistry(const TArray<GV2PackageClosure::FEntry>& ClosureEntries, FGV2ResolvedScreenRegistry& OutRegistry, FString& OutError) const`. Компиляция пишет только в локальный builder и выдаёт value после полного успеха; при отказе `OutRegistry` не меняется. `FGV2ResolvedScreenRegistry` владеет private map resolved rows: ScreenId, разрешённый UClass с GC-safe strong ownership и placement/layer policy. `Resolve` остаётся единственным способом получить class с обязательным placement; deterministic identity enumeration для fingerprint читает ту же map. Ни авторские rows, ни mutable map наружу не выдаются.

Runtime cache `ResolvedByScreenId/bBuilt` на authoring DataAsset и snapshot-pointer на этот DataAsset удаляются. Build/Resolve consumers переводятся на compile/value API; static validation helpers могут остаться на прежнем owner. Существующие правила namespace/package-root ownership, class validation, exact layer matching и hash composition не ослабляются. Strong references на разрешённые UClass допустимы: они удерживают native asset lifetime и не являются общим перестраиваемым реестром. Compile не изменяет classes/CDO или authoring inputs.

**Не считается закрытием:** отложить `ResolvedByScreenId.Reset()` до success; поставить `const` на pointer к общему UObject; скопировать wrapper с тем же pointer; восстановить A повторным Build после ошибки; `DuplicateObject` authoring registry как конечная архитектура; проверить только равенство snapshot pointers/hashes при изменившемся Resolve.

**Шаги:**
1. Создать изолированную authoring fixture, используемую обоими candidate builders; production settings живого пользовательского Editor не портить. Построить и опубликовать A, разрешить X через её PrepareContext. Запустить B через настоящий candidate path с ошибкой внутри registry compile после входа в фазу; подтвердить именно `ScreenRegistryNotReady`, затем повторить Resolve X через A. В исходном коде общая map очищается на входе Build; red-test обязан дойти до этого места, а не отказать раньше на schema/repository.
2. Реализовать compile-to-value и перенести placement/identity resolution на private resolved rows. Hash A/B строить из compiled value соответствующего snapshot, а не из `RegistryAsset->GetResolvedScreenIdentities()`. Удалить старые mutable runtime APIs и перевести callers по compile errors/reference inventory без постоянных fallback overloads.
3. Построить A/B с разными exact package closures: успешный B и отдельно B с registry ownership failure. Зафиксировать из fixture ожидаемые class/placement/availability для каждого разрешаемого screen ID; A после обоих исходов B сохраняет прежние решения. В успешном сценарии изменить authoring fixture для B так, чтобы хотя бы один class/placement/ID resolution действительно отличался; иначе два одинаковых результата не проверяют изоляцию.
4. Проверить освобождение B и garbage collection при живой A: classes, удерживаемые A, остаются доступными. Lifetime fixture не имеет иных strong roots; постоянно живой native class не доказывает корректность удержания. Уничтожение A не повреждает B. Проверить rejected placements и unknown IDs после переходов наряду с positive Resolve.
5. Создать actual ownership inventory по declarations snapshot и вложенных resolved types, включая pointer/container members, и по registry compile/resolve callers. Gate запрещает достижимый authoring registry как runtime resolver, публичный mutable compiled map и старые Build writers; неизвестная форма декларации/неописанный тип требует классификации, не молча пропускается. Для остальных snapshot authorities записать, какие references value-owned, candidate-local или shared native asset и какие production writes возможны; не объявлять всякий UObject pointer дефектом без evidence. Новое shared mutable authority требует устранения или отдельного STATUS до закрытия универсального утверждения.
6. Вернуть shared-registry pointer в temporary negative fixture и подтвердить отказ структурного gate и regression. Обновить owner contracts/Guide-like authoring reference, выполнить UBT, полный UE acceptance CFC-02 и docs validator. Сквозной StartSession A → failed B → работающий UI A затем повторяется CFC-06, где закрывается отдельный ранний host teardown.

**Done:**
- Snapshot-owned registry value не содержит ссылки на authoring DataAsset как runtime authority; compile не меняет ранее опубликованные values или authoring input.
- A сохраняет exact class, placement outcomes и availability после registry failure B и после успешного B с отличающимися inputs/package closure.
- Actual screen set берётся из compiled registry, actual placements — из `FGV2ScreenPlacement::EKind`; expected descriptors/rejections задаются независимыми fixtures, не повторным чтением A после B. До обхода проверить равенство actual keys и независимо заданного expected fixture set: пропавший при compile экран не должен исчезнуть из самого теста.
- Fingerprint identity enumeration и runtime Resolve относятся к одному compiled value; hash сам по себе не заменяет behavioral assertions.
- GC/lifetime проверены с удалением другого candidate/snapshot; resolved class pointers не висят без owning references.
- Поля и вложенные authority references actual snapshot inventory классифицированы; неизвестный member/type и возврат mutable registry дают красный gate.
- При полном закрытии удалён STATUS-020, SNAP-AF-01 получает исход CFC-04A; CFC-06 отдельно подтверждает сохранность физической UI-проекции.

**Evidence:** reached registry phase/failure code, независимые A/B expected tables и actual Resolve outcomes, разные package fingerprints, GC assertions, ownership/caller inventories, negative mutation, UBT/full UE результаты. Динамические сценарии обязательны при реализации; исходная находка подтверждена анализом кода, не объявляется уже выполненным regression run.

## CFC-04B — Зафиксировать GC ownership prepared UI и границу Game Thread

- [ ] CFC-04B — Зафиксировать GC ownership prepared UI и границу Game Thread

**Зависимость:** CFC-02A и CFC-04A. **Файлы:** `Source/GV2/Public/UI/GV2LayeredUiReconciler.h`, `GV2PropertyConsumers.h`, `GV2ScreenWidgetBase.h`, `GV2UiMutationPlan.h` в том же каталоге и их implementations; `Source/GV2PresentationApply/Private/PresentationApplyFacade.cpp`, prepared transaction/target types нижнего модуля; source ownership gate CFC-04A расширяется на actual prepared types; `Source/GV2/Private/Tests/GV2UiPrepareCommitTests.cpp`, `GV2UiCapabilityObservabilityTests.cpp`, `GV2RuntimeSubsystemTests.cpp`. Docs: `UIDocumentAndReconciliation.md`, `ScreenTemplates.md`, `BuildAndTooling.md`.

**Инвариант:** [Prepare/Commit](../../UI/UIDocumentAndReconciliation.md): prepared candidate жив до Commit/Abort, прежняя проекция — до завершения rollback/replace. Plain C++ `TObjectPtr` сам по себе не регистрирует GC ownership. Borrowed ссылки на уже удерживаемое WidgetTree не превращаются автоматически в defects.

**Решение:** lifetime owner верхней prepared transaction/reconciliation держит GC-safe strong references на off-tree candidates и объекты, нужные rollback. Borrowed targets обозначаются `TWeakObjectPtr` и проверяются на границе использования; active UMG hierarchy и UPROPERTY остаются штатным owner attached widgets. Нижний Apply-модуль не начинает владеть authoritative registries. Не превращать всё дерево plain structs в USTRUCT без traced owning container и не заменять каждый pointer strong-ссылкой: это создаст бессрочное удержание.

**Не считается закрытием:** механическая замена raw pointer на TObjectPtr; AddToRoot без ограниченного scope; GC-тест с дополнительным strong test root на кандидате; объявить текущий synchronous caller доказанным worker-thread bug только из-за отсутствия локального assert.

**Шаги:**
1. Actual reference inventory охватывает новые collection/tab widgets, top-level screens, reused/removed widgets, prepared field plans, classes и resolved resources. Для каждого указать реальный owner и срок удержания. Начальные подтверждённые gaps — off-tree `CandidateWidgetsByKey` и `FPreparedScreenInstance`, класс registry закрывает CFC-04A.
2. Через production Prepare создать новые candidates, не вставляя их в UMG tree; выполнить GC между Prepare и Commit. Проверить weak identity, фактические свойства после Commit и rollback после позднего sibling failure. Fixture не держит candidates вместо production owner.
3. Ввести scoped strong ownership на уровне готовящего/откатывающего плана; при Commit передать ownership attached tree, при Abort освободить кандидаты. Проверить GC после каждого исхода: live widgets живы, discarded candidates collectable; сохраняемая копия плана имеет определённую copy/move lifetime semantics.
4. Gate выводит actual pointer/container members prepared types и ownership classification; неизвестный member и возврат untraced owning TObjectPtr делают gate красным. Expected lifetime не выводится из текущего pointer типа: задаётся фазами contract и независимыми fixtures.
5. На `FGV2PresentationApply::Apply` добавить локальный Game Thread guard как защиту misuse, сверить actual callers. Worker-thread negative fixture запускается отдельным процессом и отвергается до widget mutation; existing Game Thread production scenario проходит. Новая async презентационная архитектура не вводится.
6. Выполнить UBT, полный UE run и red-on-revert ownership fixture; синхронизировать contracts. STATUS-021 закрывается только после CFC-04A и CFC-04B; CFC-06 повторяет верхний replacement path с GC/failure checks.

**Done:**
- Actual prepared reference inventory классифицирован по owner/lifetime; untraced owning candidate references отсутствуют.
- New/reused/removed collection/tab/screen scenarios выполняют реальные Prepare/GC/Commit/Rollback, а не только helper.
- Abort/cleanup освобождают discarded candidates; ownership не исправлен утечкой или посторонним test root.
- Восстановленные widgets сохраняют independently expected fields/placement; pointer validity одна не закрывает сценарий.
- Apply guard проверен misuse fixture и actual Game Thread callers; runtime off-thread crash без воспроизведения не заявлен.
- CFC-AF-01/03 / STATUS-021 закрыты общей GC-приёмкой; REVIEW-01 не реализован возвратом mutable registry DataAsset в качестве owner.

**Evidence:** traced owner graph, independent weak-ref/field assertions до/после GC, fault/abort matrix, negative pointer mutation, Game Thread guard fixture, UBT/full UE results.

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

**Зависимость:** CFC-04A и CFC-05. Проверка сохранности A включает прежние registry resolutions, а не только сохранение адреса её snapshot; fixture CFC-04A повторяется через верхний runtime entry point.

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
