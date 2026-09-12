---
title: C++ Foundation Readiness Audit
status: informative
version: 1.3
updated: 2026-09-12
depends_on:
  - ImplementationStatus.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../Architecture/RuntimeFacadeAndRegistries.md
  - ../Architecture/BuildAndTooling.md
---

# Аудит готовности C++ к разработке gameplay на Lua

> **Показывает:** состояние C++-основы, подтверждённые препятствия её фиксации и границы выполненной проверки.
> **Не является нормативным:** правила задают owner contracts и accepted ADR; рекомендации приёмки ниже не меняют contracts.
> **Исход:** фиксация всей C++-части не подтверждена. Рабочий gameplay-срез существует, штатные проверки проходят, но найдены нарушения lifecycle и ownership, отсутствует подключение save/load к игровому UE-host и неполна защита приёмки.

## Состояние и метод

Проверено рабочее дерево на базе `67058deac95ceefaf2b4e58b701dbd0bd6ae86d1`, включая семь уже существовавших незакоммиченных изменений: `ScreenTemplates.md`, `GV2RuntimeSubsystemTests.cpp`, `GV2WidgetSemanticFontSizeContractTests.cpp`, `PreparedPresentationTransaction.cpp`, `GV2ButtonWidgetBase.cpp/.h`, `validate_viewport_refresh_coverage.py`. Аудит их не изменял. SHA-256 входного `git diff --binary`: `47edc48d69d7adf9b3622b22b188cf1dd7a7e6e30b08c89b3e697ae28a5c31b7`. Этот результат относится к указанному рабочему дереву, а не к одному HEAD без этих правок.

Метод: маршрутизация по `Docs/README.md`, сопоставление актуальных contracts с production callers и реализацией, чтение проверок на предмет более слабого утверждения, сборки и полный зарегистрированный набор automation, дополнительные изолированные эксперименты. Архивы планов использованы как история заявленной приёмки, не как источник правил. Параллельные анализаторы не выполнили работу из-за лимита сервиса; выводы получены одним проверяющим и независимым multi-agent review не являются.

| Область | Выполнено | Предел вывода |
|---|---|---|
| Portable content, Stable ID, schemas, repository, package discovery | Сборка, все CTest/shared conformance; выборочное чтение parser identity, discovery, immutable repository и authoring write paths | Не построчное ревью всех ветвей parser/authoring; fuzzing и sanitizers не запускались |
| Lua VM и boundary | Чтение marshalling limits, protected calls, lifecycle, host services; conformance и Lua specs в обоих hosts; инъекция ошибки freeze в public `FRuntimeSession::Start` | Не доказательство отсутствия всех ошибок Lua C API и всех пользовательских комбинаций модулей |
| Session и presentation | Чтение runtime → coordinator → candidate → materializer/Prepare; полный UE-набор, включая реальные Widget Blueprint inventories и viewport tests | Новые находки переходов отмечены как анализ production-кода, если отдельного исполнения их сценария не было |
| Save/load | Чтение UE composition, portable API и storage; conformance; две записи через реальный `FFilesystemSaveSlotStorage` | Успех library-тестов не доказывает сохранение/загрузку из игрового UI |
| Приёмка | Сверка CTest/UE discovery с исполнением, CI filter, synthetic probes гейта и runner | Не проверены настройки required checks в GitHub и фактическое состояние удалённых runners |

Shipping/package/cook, платформы кроме Linux, GPU/rendered screenshot matrix, длительная игра и длительное нагрузочное тестирование не проверялись. Проверка arranged geometry не заменяет визуальную приёмку. Абсолютная корректность всей C++-части из этих результатов не следует.

## Результаты запусков

| Проверка | Результат |
|---|---|
| `cmake -S . -B cmake-build-ci -DCMAKE_BUILD_TYPE=Release` и `cmake --build cmake-build-ci --parallel 2` | success |
| `ctest --test-dir cmake-build-ci --output-on-failure` | **104/104**, failures 0 |
| `gv2-headless --self-test` | `ok=true`, Lua release `50408` |
| `gv2-headless --check-scripts` | `ok=true`, `modules_checked=45` |
| `gv2-content validate GameData/core` | exit 0 |
| UBT `GV2Editor Linux Development -WaitMutex -NoHotReloadFromIDE` | `Result: Succeeded` |
| MCP `DiscoverTests` / `ListTests(limit=0)` / `RunTestsByFilter(StartsWith:GV2)` | Обнаружено 141, выполнено **141/141**, failed/skipped 0 |
| Свежий `UnrealEditor-Cmd`, полный `GV2`, `-nullrhi`, `-ReportExportPath` | 141 test records со state `Success`; `succeededWithWarnings=141`, `failed=notRun=inProcess=0`, error entries 0; 167 warnings |
| Сверка имён fresh-report с MCP discovery | Missing 0, extra 0 |

Предупреждения fresh-process включают `Querying IsUsingWayland before SDL is initialized` и диагностические сообщения негативных fixtures; это не прогон без предупреждений. Старый процесс Editor после перелинковки держал прежний mapped `GV2PresentationApply.so`, поэтому результат MCP дополнен отдельным свежим процессом. Пользовательский Editor не завершался и не перезапускался.

Локальные, некоммитимые evidence находятся в `Saved/Audit/CppFreezeReadiness/`: `input_state.json`, `input_changes.patch`, `ue_discovered.json`, `ue_results.json`, `FreshReport/index.json`, `ue_fresh.log`, `headless_self_test.txt`, `check_scripts.json`, `runtime_probe.cpp`, `runtime_probe_result.txt`, `verification_probes.py`, `verification_probe_results.json`. Эти артефакты могут быть удалены при очистке `Saved`; существенные входы и результаты экспериментов сохранены ниже. Portable test log — `cmake-build-ci/Testing/Temporary/LastTest.log`.

## Счёт и интерпретация

Первоначальный аудит дал девять находок: шесть P1 и три P2; семь contract gaps перенесены в `STATUS-013…019`, две находки организации приёмки остаются открытыми здесь. Дополнительная проверка внешнего review 2026-09-12 подтвердила SNAP-AF-01 (закрыта задачей CFC-04A, STATUS-020 удалён). Находка PSC-AF-03 также закрыта задачей CFC-04 (STATUS-013 удалён). Перенос в status означает фиксацию расхождения для планирования, а не исправление кода. Результаты запусков выше относятся к первоначальному аудиту; дополнение ниже основано на анализе кода и не является повторным полным test run.

### Snapshot ownership — дополнение внешнего review

#### SNAP-AF-01 — P1 — snapshot разделяет mutable Screen Registry с новым candidate

**Источник:** внешнее review пользователя, имя SNAP-R1. Независимо сверено по production-коду ревизии `035ac04` (2026-09-12); рабочее дерево перед проверкой чистое.

**Норма:** [ADR-0043 D1](../ADR/0043-presentation-apply-boundary.md), [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md): snapshot неизменяем, authorities принадлежат candidate/session, подготовка B и её отказ до commit-to-replace не меняют опубликованную A.

**Подтверждение:** `Source/GV2/Private/Application/GV2SessionContentSnapshot.h:20–22` хранит `TStrongObjectPtr<UGV2ScreenRegistry>` внутри resolved wrapper. В `GV2SessionContentSnapshot.cpp:124–142` candidate загружает configured DataAsset, вызывает `RegistryAsset->Build(ClosureEntries, ...)` и сохраняет ссылку на тот же asset. Wrapper `Resolve` делегирует этому объекту (`:24–36`). `Source/GV2/Private/UI/GV2ScreenRegistry.cpp:249–252` при каждом Build сначала очищает `ResolvedByScreenId` и выставляет `bBuilt=false`; отказ из проверок ниже оставляет этот объект очищенным. Успех заменяет ту же map (`:350–351`), а Resolve читает её с учётом `bBuilt` (`:363`). Strong pointer обеспечивает lifetime, но не изоляцию.

**Следствие:** после успешной A registry failure B способен сделать ранее допустимый Resolve A неизвестным; при успешном rebuild B A читает его map вместо своего compiled value. Разные package closures также проходят через общий mutable объект. Комментарий о безопасности повторного Build с тем же closure не покрывает failure и разные inputs. Это отдельный дефект от STATUS-014 (выбор неправильного snapshot) и STATUS-015 (раннее разрушение UI): правильная ссылка на A не сохраняет её содержимое.

**Граница evidence:** shared ownership и destructive-before-validation path подтверждены исходниками. Динамический A→failed B и A→successful B с разными closures в этой проверке не запускались; они обязательны для закрытия задачи. Остальные замечания внешнего review уже отражены в текущем аудите/плане; их положительная общая оценка не заменяет новую приёмку всей C++-поверхности.

**Проверка для закрытия:** authoring DataAsset компилируется read-only в независимый resolved registry value каждого snapshot; fingerprint и Resolve читают именно его. Published A сохраняет exact descriptor/class/placement outcomes после registry failure B и успешного B с различающимися inputs/closures. Дополнительно проверяются strong class ownership при GC и actual inventory вложенных authority references; `const` wrapper или shallow pointer copy недостаточны. Исполнение — [CFC-04A](../Plans/CppFoundationClosure/SessionLifecycle.md#cfc-04a-сделать-resolved-screen-registry-независимым-значением-snapshot); верхний UI replacement повторяет CFC-06.

**Исход:** *(Закрыто задачей CFC-04A)* Авторский `UGV2ScreenRegistry` переведён в статус строго входного `const` DataAsset без мутируемого кэша, а `FGV2SessionContentSnapshot` теперь владеет независимым значением `FGV2ResolvedScreenRegistry` с GC-safe `TStrongObjectPtr<UClass>`, изолированным от мутаций и ошибок других сессий (подтверждено тестом `FGV2SessionScreenRegistrySnapshotIsolationTest` и статическим гейтом `validate_session_snapshot_ownership.py`).

### PresentationStructuralClosure

#### PSC-AF-03 — P1 — materializer использует второй глобальный авторитет UI-схем

**Норма:** [Bootstrap and Session Lifecycle, Cold start](../Architecture/BootstrapAndSessionLifecycle.md#cold-start): UI-схемы принадлежат одному candidate/snapshot; обнаружение происходит один раз за сессию. [Screen Templates](../UI/ScreenTemplates.md): production Prepare использует контекст pinned snapshot.

**Подтверждение по production-коду:** `Source/GV2/Private/Application/GV2SessionContentSnapshot.cpp` создаёт `Snapshot.SchemaCache` и вызывает `CompileAll()`. Но `Source/GV2/Private/Application/GV2SessionCoordinator.cpp`, строка 307, затем вызывает `RebuildSchemaCacheForSession`. `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp`, строки 26, 578–608, 649, 681, хранит отдельный process-global `GSessionSchemaCache`; top-level fields, binding schemas и nested envelopes читают его. `PrepareContext` передаётся далее для части операций, но schema lookup его snapshot не использует. Конструктор каждого `Source/GV2/Private/UI/GV2UiSchemaCache.cpp` выполняет собственный filesystem discovery.

**Следствие:** схема, проверенная внутри candidate, и схема, используемая materializer, являются разными экземплярами, полученными двумя чтениями. При изменении источника между чтениями возможны разные правила валидации; глобальный кэш также не принадлежит конкретному snapshot. Сам факт двух production authorities подтверждён независимо от воспроизведения гонки файловой системы. Гонка в UE отдельно не исполнялась.

`ContentSnapshotContract` проверяет наличие схемы в snapshot, а `SchemaCacheSessionScoping` перестраивает глобальный кэш напрямую; ни один из этих сценариев не доказывает, что production materializer пользуется первым. Это конкретный случай «механизм есть, но вызывающий использует другой».

**Проверка для закрытия:** production document с binding/nested envelope получает compiled schema именно своего snapshot; другой построенный candidate и изменение исходных файлов не меняют результат. Actual set — все schema lookup callers materializer/consumer из исходников, oracle — identity схемы pinned snapshot. Второй global cache/discovery path отсутствует.

**Исход:** *(Закрыто задачей CFC-04)* Процесс-глобальный кеш схем и методы Rebuild/ReleaseSchemaCacheForSession полностью удалены, все точки входа materializer требуют обязательный `FGV2PresentationPrepareContext` и читают схемы исключительно из snapshot-scoped кеша, изолированного от изменений файлов и других сессий (подтверждено тестом `FGV2SessionUiSchemaSnapshotIsolationTest` и структурным гейтом `validate_ui_schema_authority.py`).

#### PSC-AF-04 — P1 — повторный StartSession подготавливает первый документ через прежний snapshot

**Норма:** [Bootstrap and Session Lifecycle, New/load session build](../Architecture/BootstrapAndSessionLifecycle.md#newload-session-build): initial document применяется против private candidate; старый snapshot не наследуется.

**Подтверждение по production-коду:** в `Source/GV2/Private/Application/GV2SessionCoordinator.cpp` существующий `ContentSnapshot` не сбрасывается после остановки прежней VM. Затем устанавливается `InProgressCandidate`, вызывается document pipeline, и лишь после его успеха выполняется `ContentSnapshot = MoveTemp(Candidate)`. Однако `Source/GV2/Private/Application/GV2SessionCoordinator.h`, строки 65–68, предпочитает `ContentSnapshot`, если он непустой. Production `PrepareDocumentRequest` и runtime screen resolution используют этот getter.

**Сценарий:** успешная сессия A → повторный `StartSession` на том же coordinator без `EndSession` → initial Prepare сессии B получает A. При отличающихся screen/resource/theme данных возможны отказ по старому реестру либо применение старых значений, после которого публикуется B. Даже при одинаковом контенте нарушена identity candidate. Это вывод по конкретной ветви; динамический переход A→B с изменённой темой в этом аудите не исполнялся.

**Почему зелёный тест не опровергает находку:** `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`, строки 1492–1578, запускает одну сессию, а второй candidate строит отдельно. Второго успешного `StartSession` там нет; комментарий теста явно это оговаривает.

**Проверка для закрытия:** два настоящих успешных `StartSession` с различающимися authority values; внутри второго DocumentSink Prepare-context соответствует B, внешний snapshot не выдаёт A за готовую B. Отдельный cold-start тест этого не заменяет.

**Исход:** *(Закрыто задачей CFC-06)* Методы `GetContentSnapshotForPrepare` и `InProgressCandidate` удалены; `FDocumentSink` получает явный `const FGV2PresentationPrepareContext&`, а в `BeginReplace` старый snapshot сбрасывается до вызова sink. Два последовательных вызова `StartSession` на одном координаторе с разными наборами пакетов (SetA и SetB) подтвердили, что initial document сессии B готовится строго против authorities B, а координатор во время sink B не возвращает старый снимок A (`FGV2SequentialSessionsDoNotShareAuthoritiesTest`, `STATUS-014` удалён).

#### PSC-AF-05 — P1 — UE-host разрушает текущую проекцию до отказоспособной проверки replacement

**Норма:** [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md): отказ private content candidate до commit-to-replace сохраняет прежнюю Ready-сессию и её опубликованное состояние.

**Подтверждение по production-коду:** `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, строки 239–262, сначала удаляет `ActiveScreen`/`ActiveGameShell`, сбрасывает reconciler и создаёт новый shell, и только затем вызывает coordinator. Его `FailReplacementAttempt(..., true)` сохраняет прежнюю Ready-сессию при ошибке candidate. Обратного восстановления прежнего shell/runtime projection в этой ветви host-а нет; recovery surface показывается только при `ApplicationState::Failed`.

**Сценарий и следствие:** Ready A → повторный публичный StartSession → ошибка Screen Registry/Image Catalog/Theme candidate. Coordinator и bindings остаются A, но её физический UI уже удалён. Проверка сохранности coordinator не доказывает сохранность продукта. Дополнительно GameShell class здесь загружается из settings до candidate, хотя его identity уже входит в snapshot. Живой отказ replacement через UI отдельно не инжектировался; вывод основан на порядке фактических вызовов.

**Проверка для закрытия:** inject content-builder failure через `UGV2RuntimeSubsystem`, сравнить до/после identity и геометрию прежнего shell, экраны, работоспособность bindings и session generation. Actual set стадий отказа должен выводиться из candidate-build stages. Проверить источник класса нового shell в успешной ветви.

**Исход:** *(Закрыто задачей CFC-06)* Из `UGV2RuntimeSubsystem::StartSession` удалены преждевременный teardown `ActiveScreen`/`ActiveGameShell` и синхронная загрузка `GameShellClass` из настроек; teardown выполняется через `ProjectionTeardownSink` внутри `BeginReplace`, GameShell инстанциируется из `PrepareContext.GetGameShellClass()`, удерживается вне viewport как `PendingGameShell` и публикуется в viewport только в `PublishActiveProjection` внутри `PublishReady`. Отказ candidate до `BeginReplace` оставляет активный UI сессии A, её viewport attachment и интерактивность полностью сохранными (`FGV2SessionPreservesProjectionWhenCandidateFailsTest`, гейт `validate_session_replacement_ownership.py`, `STATUS-015` удалён).

#### PSC-AF-06 — P2 — гейт Build.cs молча пропускает запрещённую зависимость в другой форме C#

**Норма:** [Build and Tooling, Presentation structural gates](../Architecture/BuildAndTooling.md#presentation-structural-gates), [ADR-0043](../ADR/0043-presentation-apply-boundary.md): graph inventory защищает Apply allowlist независимо от имени будущего authority.

**Эксперимент:** в temporary копию действующего Build.cs добавлена одна строка, вызван полный `validate_repository()` из `Tools/Testing/validate_presentation_apply_module_graph.py`. Менялась только форма массива:

```csharp
PrivateDependencyModuleNames.AddRange(new string[] { "GV2ContentCore" }); // отклонено
PrivateDependencyModuleNames.AddRange(new[] { "GV2ContentCore" });        // violations=[]
```

Baseline и штатный self-test зелёные. Парсер `extract_dependency_modules`, строки 71–84, распознаёт ограниченную форму и не сигнализирует о пропущенном dependency statement. Запрещённой зависимости в текущем Apply-модуле нет; это подтверждённая неполнота гейта, а не заявление о существующей обратной ссылке или выполненная компиляция synthetic Build.cs.

**Исход:** *(Закрыто задачей CFC-03)* Статический regex заменён на fail-closed парсер C# для `GV2PresentationApply.Build.cs`, проверяющий 100% токенов/выражений, отвергающий любые вспомогательные методы, циклы, ветвления, сторонние include paths и custom base classes, поддерживающий `new string[]`, `new[]` и `.Add()`, проверяющий forward edge `GV2 -> GV2PresentationApply` и единственный consumer, а также вычисленный CMake File API codemodel и изоляцию authority в UBT.

### Runtime foundation

#### RUNTIME-AF-01 — P1 — ошибка freeze реестра не блокирует startup

**Норма:** [Runtime Facade and Registries, Host-side freeze sequence](../Architecture/RuntimeFacadeAndRegistries.md#host-side-freeze-sequence): ошибка freeze и незамороженный required registry перед state build являются startup fault.

**Код:** `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, строки 1711–1733, возвращает `void`, игнорирует результат `lua_pcall`, не проверяет `is_frozen`; `RunLifecycleHooks` продолжает работу. Результат `freeze_reference_fields` в соседней ветви также не потребляется.

**Воспроизведено через public C++ entry point:** temporary executable связан с собранными `libgv2_runtime_core.a` и `libgv2_content_core.a`. `BuildRepository` создаёт разрешённый пустой core snapshot. Единственный модуль manifest — `core:module.bootstrap.main`, `source="bootstrap/main.lua"`, `dependencies={}`. Его `register` содержит synthetic registry:

```lua
game.services = {
    freeze = function() error("AUDIT_FREEZE_FAILURE") end,
    is_frozen = function() return false end,
}
game.runtime.get_canonical_state_hash = function()
    return game.services.is_frozen() and "frozen" or "unfrozen"
end
```

Реальный `FRuntimeSession::Start` вернул `true`, fault code остался пустым, accessor вернул `unfrozen`. Строка результата: `freeze_exception: started=1 fault= registry=unfrozen`. Это fixture C++-оркестрации, не копия gameplay-правила и не нарушение malicious-code sandbox: штатный host обязан потреблять отказ своего lifecycle-вызова.

**Проверка для закрытия:** ошибка, отсутствующий required registry, missing freeze и false `is_frozen` не дают перейти к state build/start; отказ содержит registry identity. Actual registry set должен происходить из production lifecycle declarations, а fault injections проходить через `FRuntimeSession::Start` в обоих hosts.

**Исход:** *(Закрыто задачей CFC-05)* Фаза freeze заменена на обязательную фазу sealing через `core:module.bootstrap.registry_lifecycle`, где ошибка freeze, отсутствие участника контракта или false из `is_frozen()` гарантированно прерывают startup типизированным fault с указанием пути реестра, фасад защищён от ad hoc мутаций metatable-барьером, а C++-оркестрация и Lua-спецификация проверяют невозможность перехода к state build.

### SaveAndLoad

#### SAV-AF-01 — P1 — save/load библиотека не подключена к игровой UE-сессии

**Норма:** [Overview, Vertical slice acceptance](../Architecture/Overview.md#vertical-slice-acceptance), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md): сохранение и восстановление gameplay проходят через host slot storage и session lifecycle.

**Подтверждение по коду:** production `Source/GV2/Private/Application/GV2SessionCoordinator.cpp` запускает только `RuntimeSession.Start`. В production UE-дереве отсутствуют вызовы `SetSaveSlotStorage` и `StartFromSave`; найденный UE `SetSaveSlotStorage` находится в `GV2LuaSpecRunnerHostTests.cpp`. Storage implementation существует и тестируется, но composition root игрового host-а её не предоставляет. `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, строки 399–403, без неё возвращает `false, "unavailable"`; существующий Lua `save.save` преобразует это в `SaveWriteFailed:unavailable` при достижении storage после safe-point проверки.

**Следствие:** green save conformance не означает, что игровой UE-host сохраняет прохождение или начинает его из слота. Для подключения необходима C++-работа, поэтому безусловная фиксация C++ перед Lua gameplay преждевременна. Optional storage у library `Start` допустим; находка относится к отсутствующему end-to-end продукту, а не к дефекту optional API. Общий replacement flow уже учтён в `STATUS-001`; здесь отдельно фиксируется отсутствующая host storage wiring.

**Проверка для закрытия:** реальная UE-сессия получает storage, сохраняет достигнутое командой состояние в safe point, загружается через production entry point и продолжает игру с теми же state hash/instance IDs. Прямой вызов storage из теста и отдельная portable load-сессия недостаточны.

**Исход:** подтверждено как contract gap, [STATUS-018](ImplementationStatus.md).

#### SAV-AF-02 — P2 — успешная перезапись слота не сохраняет предыдущую копию

**Норма:** accepted [ADR-0021, Decision](../ADR/0021-opaque-save-container.md#decision), [Canonical State and Save, Export boundary](../Architecture/CanonicalStateAndSave.md#export-boundary): host сохраняет предыдущую копию наряду с атомарной подменой.

**Воспроизведено:** в новом temporary каталоге два вызова реального `Source/GV2RuntimeCore/Private/GV2SaveSlotStorage.cpp` записали `previous_payload`, затем `current_payload` в `audit_slot`. Оба вернули `Ok`; обход всех regular files каталога нашёл один файл с текущими байтами и ни одной предыдущей копии. Вывод: `writes_ok=1 current=current_payload files=1 previous_copy_exists=0`. Реализация выполняет только `rename(temp, slot)`; backup path отсутствует.

Сохранность старого слота при неуспешной записи — другое свойство, и оно покрыто существующим conformance. Наблюдение не утверждает отсутствие atomic replace и не является power-loss тестом. Более узкое описание atomic write в BuildAndTooling не отменяет требования accepted ADR о предыдущей копии.

**Проверка для закрытия:** после успешной замены доступны новые bytes и предыдущая valid copy; fault injection по стадиям ротации не уничтожает обе. Ожидаемые bytes задаёт fixture независимо от implementation. Формат контейнера остаётся непрозрачным для host-а.

**Исход:** подтверждено как contract gap, [STATUS-019](ImplementationStatus.md).

### Project verification

#### VERIFY-AF-01 — P1 — CI пропускает 20 зарегистрированных тестов проекта

**Evidence:** `.github/workflows/linux-ci.yml`, строка 76, выполняет `Automation RunTests GV2.Runtime`. Из 141 имени текущего MCP discovery 20 не имеют этого префикса: `GV2.UI.*` и `GV2.Editor.*`. Среди них `GV2.UI.PrepareCommitAndFailureInjection`, `GV2.UI.LayeredReconciliationContract`, `GV2.UI.StandardPropertyConsumers`. Проверка job читает строки лога и не сопоставляет множество запрошенных/завершённых test records.

Это повторно подтверждённый риск приёмки, но **не** расхождение workflow с нынешним Integration gate contract: [BuildAndTooling](../Architecture/BuildAndTooling.md#integration-gate) сам требует узкий `GV2.Runtime`. Поэтому запись не добавляется в таблицу contract ↔ code gaps.

*(Закрыто задачей CFC-02)* CI и local acceptance переведены на запуск всего набора `GV2` (включая UI и Editor) через fresh-process runner `run_ue_acceptance.py` со строгой сверкой discovery inventory и report records.

#### VERIFY-AF-02 — P2 — локальный UE-runner сообщает успех при NotRun

**Эксперимент:** `Tools/MCP/run_ue_tests.py`, строки 123–126, принимает успех по `failed == 0 and total > 0`, игнорируя накопленный `failed_tests`, `skipped` и равенство passed/total. Подстановка ответа клиента `total=1, passed=0, failed=0, skipped=1`, единственная запись `state="NotRun"`, приводит к:

```text
[FAIL] GV2.Audit.Synthetic
SUCCESS: All 0/1 tests passed.
exit=0
```

Это synthetic test самого runner, не результат реального UE automation текущего аудита. Реальные отчёты разобраны и сопоставлены отдельно. Дополнительно текущий `mcp_client.py` с default `requests.Response.iter_lines()` задерживал короткий SSE-ответ до timeout; в audit-process использован `chunk_size=1`, исходник клиента не менялся.

*(Закрыто задачей CFC-02)* В `ue_test_report.py` реализована fail-closed валидация `validate_run`, подключённая к `run_ue_tests.py` и `run_ue_acceptance.py`, исключающая пропуск non-success состояний (NotRun, InProcess, Fail) и расхождений счётчиков.

### CppFullCodeReview — проверка REVIEW-01…15

Источник — предоставленный `Docs/Status/CppFullCodeReview.md`; исходные формулировки не являются нормой. Сверено с source HEAD `78e96f1` 2026-09-12. Входные незакоммиченные review и `Docs/README.md` не изменялись. Применён последовательный анализ кода/owner contracts, для Value/manifest/digest — отдельный compiled probe против portable libraries из `build/Source`. Полный suite, UE GC, thread death tests, OOM и randomized gameplay replay в этой дополнительной проверке не запускались. Заявленные исходным review пять параллельных проверок и 104 tests здесь не выдаются за наше новое evidence.

Добавлено 15 adjudication blocks CFC-AF-01…15: девять содержат подтверждённый дефект или более узкий механизм риска (01…07, 09, 10), пять отклоняют заявленную correctness/performance проблему (08, 11…14), один фиксирует только форматирование (15). Пять новых contract gaps — STATUS-021…025; test hygiene и defensive guards отдельно не объявляются прежними runtime contract violations. Вместе с прежними десятью записями документ содержит 25 finding blocks; это не 25 новых открытых bugs. Утверждение исходного review «архитектура строго соответствует инвариантам» не подтверждается при существующих STATUS и обнаруженном native state composition.

#### CFC-AF-01 — REVIEW-01 — P1 — GC ownership registry classes

**Подтверждено по коду:** `GV2ScreenRegistry.h` хранит raw `UClass*` в plain `FResolvedScreen`, map не traced; authoring `TSoftClassPtr` после `LoadSynchronous` не становится owning class reference. Snapshot strong-pointer удерживает registry, но не исправляет его непрослеживаемую map. Crash после GC не воспроизведён в этом раунде. Норма — lifetime resolved value для [snapshot/Prepare](../Architecture/BootstrapAndSessionLifecycle.md).

**Исход:** [STATUS-021](ImplementationStatus.md), CFC-04A. Предложенное review возвращение runtime map на UPROPERTY DataAsset не принимается как архитектура: оно противоречит выбранному compile-to-value устранению STATUS-020. GC-safe class ownership уже включено в CFC-04A и проверяется вместе с CFC-04B.

#### CFC-AF-02 — REVIEW-02 — P2 — fixtures оставляют rooted GameInstance

*(Закрыто задачей CFC-02A)* Введён RAII-владелец `FScopedTestWorldContext`, гарантирующий вызов `Shutdown()`, удаление из root и уничтожение `UWorld`/`FWorldContext` в `GEngine`, тестовые вызовы `AddToRoot` переведены на scoped context и защищены статическим гейтом `validate_test_fixture_ownership.py`.

#### CFC-AF-03 — REVIEW-03 — P1 — off-tree candidates без traced owner

*(Закрыто задачей CFC-04B)* Зафиксирован явный GC ownership off-tree candidates (`TStrongObjectPtr` в `CandidateWidgetsByKey` коллекций/табов и `CandidateWidget` реконсилера) с освобождением при Commit/Reset, borrowed targets переведены на `TWeakObjectPtr`, добавлен Game Thread guard в `FGV2PresentationApply::Apply`, а соблюдение структуры защищено расширенным статическим гейтом `validate_session_snapshot_ownership.py`.

#### CFC-AF-04 — REVIEW-04 — P1 — seed не достигает session bootstrap

*(Закрыто задачей CFC-07A)* Введён типизированный `FSessionStartInputs` с 16-символьным hex seed, изолированным от session generation и передаваемым в Lua (`game.runtime.seed_hex`) до первого bootstrap-хука, реализован Lua-owned xoshiro128** PRNG (`game.random`) с сериализацией в `state.meta.prng`, манифесты и дайджесты переведены на формат v2, а детерминизм и изоляция проверены в обоих хостах.

#### CFC-AF-05 — REVIEW-05 — P1 — native code интерпретирует canonical state

*(Закрыто задачей CFC-05A)* Сборка canonical state и слияние вкладов модулей перенесены целиком в Lua (`core:module.runtime.state_composition`), из C++ удалены `MergeStateContribution` и `IsCanonicalStateSection`, а разделение защищено статическим гейтом `validate_state_composition_ownership.py`.

#### CFC-AF-06 — REVIEW-06 — P2 — canonical zero; NaN-часть отклонена

*(Закрыто задачей CFC-03A)* Конструктор `FValue(double)` нормализует negative zero (`-0.0` → `+0.0`), обеспечивая равенство значений и идентичность canonical SHA-256 хэшей при сохранении разных kinds для Integer 0 и Number 0.0, а non-finite double по-прежнему отвергаются исключением `std::invalid_argument`.

#### CFC-AF-07 — REVIEW-07 — P2 — Digest принимает неканонические hash strings

*(Закрыто задачей CFC-03A)* Введён единый предикат `IsCanonicalSha256`, проверяющий ровно 64 lowercase ASCII hex-символа для всех хэш-полей Manifest и Digest (с разрешённым пустым `state_hash`), а соответствие полей подтверждено структурным гейтом `validate_headless_hash_fields.py`.

#### CFC-AF-08 — REVIEW-08 — отсутствие локального Game Thread assertion

**Подтверждено только отсутствие guard:** actual `FGV2PresentationApply::Apply` начинается на строке 249, не 684; локального IsInGameThread assertion нет. Production coordinator Start/ingress guards Game Thread, а evidence вызова Apply worker-ом review не содержит.

**Исход:** *(Отклонено)* как подтверждённый P2 production thread violation. Наблюдаемое условие повторного открытия — новый worker/asynchronous caller в actual Apply call inventory либо failed thread-affinity test. CFC-04B добавляет bounded defensive guard и misuse test; это не новая async architecture и не доказательство ранее случавшегося UB.

#### CFC-AF-09 — REVIEW-09 — P2 — fixed temporary slot path при concurrent writers

**Подтверждён collision mechanism:** `GV2SaveSlotStorage.cpp:94` использует один `.tmp` для slot и не синхронизирует отдельные writers. Две concurrent записи могут столкнуться; concurrent product caller и испорченный файл в этом раунде не воспроизведены. Обещание atomic replace описано [storage contract](../Architecture/BuildAndTooling.md); actual concurrency boundary должен быть явным.

**Открыто, уже покрыто:** CFC-08 требует exclusive owner lock, serialization и process/concurrent tests плюс atomic publication Current/Previous. Unique filename само по себе не определяет порядок commits/Previous и не заменяет этот protocol. Новый STATUS, дублирующий уже запланированную storage boundary, не добавляется без отдельного подтверждённого product concurrency нарушения.

#### CFC-AF-10 — REVIEW-10 — P2 — forgery mode не восстанавливается

*(Закрыто задачей CFC-02A)* Введён RAII-класс `FScopedForgeryMode`, сохраняющий и восстанавливающий прежнее состояние режима, прямой вызов мутации закрыт статическим гейтом, а созданный виджет фиксирует режим при конструировании.

#### CFC-AF-11 — REVIEW-11 — смешение shared pointer families

**Исход:** *(Отклонено)* как correctness gap. `GV2BridgeTypes.h` соединяет UE prepared value и portable compiled schema; разные pointer families отражают модульную границу, причина non-UPROPERTY уже описана рядом с полями. Pimpl ради единообразия здесь не нужен. Наблюдаемое условие повторного открытия — ошибка ownership/conversion либо изменение portable API, создающее реальную зависимость от UE types.

#### CFC-AF-12 — REVIEW-12 — размер RuntimeCoreTests

**Исход:** *(Отклонено)* как обязательный foundation blocker: `wc -l` на проверенной ревизии показывает 2612 строк, не 10K+. Ни compilation regression, ни failure-localization metric не приведены. Новые tests CFC-задач размещаются по owner-у; blanket split файла не требуется. Наблюдаемое условие повторного открытия — измеренный compile-time regression этого translation unit или конкретная воспроизводимая проблема test ownership/discovery.

#### CFC-AF-13 — REVIEW-13 — Queue.Empty как оптимизация

**Исход:** *(Отклонено)* как доказанная performance/correctness проблема. RuntimeIngressQueue.Reset корректно dequeue-ит и сбрасывает QueueSize. Локальный UE `Containers/Queue.h:110` реализует Empty через цикл Pop, а не constant-time освобождение. Замена могла бы убрать перемещение Item, но измеренного bottleneck нет. Условие повторного открытия — профиль, показывающий существенную стоимость Reset/item move, либо неверный queue size после reset.

#### CFC-AF-14 — REVIEW-14 — synchronous asset load на старте

**Исход:** *(Отклонено)* как самостоятельный performance defect без измерения: [bootstrap contract](../Architecture/BootstrapAndSessionLifecycle.md) прямо допускает synchronous pre-VM candidate build. Неправильный отдельный GameShell settings lookup уже устраняется CFC-06/STATUS-015; новый async loader из замечания не следует. Условие повторного открытия — измеренное нарушение принятого loading-time budget либо обнаруженный LoadSynchronous на Ready/Apply пути.

#### CFC-AF-15 — REVIEW-15 — indentation lifecycle block

*(Закрыто задачей CFC-05A)* Блок жизненного цикла фазы 3 в `GV2RuntimeSession.cpp` заменён вызовом `ComposeDefaultCanonicalStateTree` и отформатирован в едином стиле в рамках переноса State Composition.

### Evidence дополнительной portable проверки

Локально: `Saved/Audit/CppFullReviewVerification/value_digest_probe.cpp` и executable; входные SHA-256 review/router — `input.json`. Probe вызвал реальные public constructors/hash/codecs, linked `build/Source/libgv2_runtime_core.a` и `libgv2_content_core.a`. Существенные результаты приведены в CFC-AF-06/07 и не зависят от сохранности временного каталога. Это не замена полного native/UE acceptance и не утверждение, что отредактированы или исправлены исходники.

## Ранее известные ограничения

- `STATUS-001`: replacement/preflight/cancellation/session lifecycle не завершён. Portable cold-start load существует; UE product load из этого не следует.
- `STATUS-002` и `STATUS-003`: effects и enter/exit animation paths отсутствуют. Они не препятствуют всякой gameplay-разработке, но замораживать C++ с обещанием этих возможностей нельзя.
- `STATUS-011`: обязательность данных сцены не определена; это content/schema вопрос, который следует решить до массового authoring.
- [PresentationModel](../Concepts/PresentationModel.md) всё ещё говорит об одном экране и нереализованном UI document; [CanonicalStateAndSave](../Architecture/CanonicalStateAndSave.md) одновременно содержит старую запись о неготовых migrations и отдельный раздел реализованных migrations. Валидатор links/front matter не проверяет такие смысловые противоречия. Эти описания не использованы как evidence отсутствия реально существующего кода.

## Рекомендация по фиксации C++

Обоснованное решение на текущем состоянии — **не объявлять всю C++-часть корректной и завершённой**. Продолжать экспериментальный Lua gameplay на существующем срезе можно, но это не равнозначно фиксации инфраструктуры. Новую большую перестройку этот аудит не предписывает: сначала устранить конкретные нарушения существующих contracts.

Рекомендуемая последовательность приёмки:

1. Закрыть `STATUS-013…017`, `STATUS-020…025`, VERIFY-находки и test-hygiene findings: единственные изолированные authorities, GC lifetime, верный candidate при повторном запуске, сохранность UI, Lua-owned state composition, seed input, canonical values/codecs и достоверная приёмка.
2. Завершить необходимый для игры save/load путь (`STATUS-001`, `018`, `019`), проверить его через UE composition root и восстановление после отказов. Зелёная библиотечная функция не заменяет продуктовый сценарий.
3. Ограничить обещанную первую gameplay-версию: effects/animations и остальные ещё отсутствующие host capabilities либо реализуются, либо явно остаются за её пределами; отсутствие функции не выдаётся за её готовность.
4. На чистой зафиксированной ревизии повторить portable + полный UE gate и один вертикальный сценарий: старт → команда → canonical mutation/event → presentation → save → load → продолжение команды. Ожидаемые состояние, bindings и geometry проверяются независимо; transitions действительно исполняются. Для Shipping отдельно нужны cook/package и smoke запуска установленного продукта.
5. После этого фиксировать конкретную поддержанную поверхность C++/Lua и evidence baseline. Расширение capabilities, зависимостей и lifecycle впоследствии проходит тот же gate. Обещания «C++ больше никогда не меняется» и абсолютного отсутствия дефектов проверками не подтверждаются.

Аудит завершён как обследование указанного состояния. Исправление находок не выполнялось, готовность C++ не отмечалась, планы и архивы не закрывались.

Исполняемая декомпозиция устранения причин и приёмки Lua gameplay baseline: [C++ Foundation Closure](../Plans/CppFoundationClosure/README.md). Создание плана не меняет исходы находок и не подтверждает их исправление.
