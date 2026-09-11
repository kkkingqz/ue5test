---
title: C++ Foundation Readiness Audit
status: informative
version: 1.0
updated: 2026-09-11
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

Девять находок: шесть P1 и три P2. Семь подтверждённых contract gaps перенесены в `STATUS-013…019`; две находки организации приёмки остаются открытыми здесь. Перенос в status означает фиксацию расхождения для планирования, а не исправление кода. Гейт зависимостей и неполный CI-filter уже были обнаружены в предшествующей оценке и в этом аудите перепроверены.

### PresentationStructuralClosure

#### PSC-AF-03 — P1 — materializer использует второй глобальный авторитет UI-схем

**Норма:** [Bootstrap and Session Lifecycle, Cold start](../Architecture/BootstrapAndSessionLifecycle.md#cold-start): UI-схемы принадлежат одному candidate/snapshot; обнаружение происходит один раз за сессию. [Screen Templates](../UI/ScreenTemplates.md): production Prepare использует контекст pinned snapshot.

**Подтверждение по production-коду:** `Source/GV2/Private/Application/GV2SessionContentSnapshot.cpp` создаёт `Snapshot.SchemaCache` и вызывает `CompileAll()`. Но `Source/GV2/Private/Application/GV2SessionCoordinator.cpp`, строка 307, затем вызывает `RebuildSchemaCacheForSession`. `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp`, строки 26, 578–608, 649, 681, хранит отдельный process-global `GSessionSchemaCache`; top-level fields, binding schemas и nested envelopes читают его. `PrepareContext` передаётся далее для части операций, но schema lookup его snapshot не использует. Конструктор каждого `Source/GV2/Private/UI/GV2UiSchemaCache.cpp` выполняет собственный filesystem discovery.

**Следствие:** схема, проверенная внутри candidate, и схема, используемая materializer, являются разными экземплярами, полученными двумя чтениями. При изменении источника между чтениями возможны разные правила валидации; глобальный кэш также не принадлежит конкретному snapshot. Сам факт двух production authorities подтверждён независимо от воспроизведения гонки файловой системы. Гонка в UE отдельно не исполнялась.

`ContentSnapshotContract` проверяет наличие схемы в snapshot, а `SchemaCacheSessionScoping` перестраивает глобальный кэш напрямую; ни один из этих сценариев не доказывает, что production materializer пользуется первым. Это конкретный случай «механизм есть, но вызывающий использует другой».

**Проверка для закрытия:** production document с binding/nested envelope получает compiled schema именно своего snapshot; другой построенный candidate и изменение исходных файлов не меняют результат. Actual set — все schema lookup callers materializer/consumer из исходников, oracle — identity схемы pinned snapshot. Второй global cache/discovery path отсутствует.

**Исход:** подтверждено как contract gap, [STATUS-013](ImplementationStatus.md).

#### PSC-AF-04 — P1 — повторный StartSession подготавливает первый документ через прежний snapshot

**Норма:** [Bootstrap and Session Lifecycle, New/load session build](../Architecture/BootstrapAndSessionLifecycle.md#newload-session-build): initial document применяется против private candidate; старый snapshot не наследуется.

**Подтверждение по production-коду:** в `Source/GV2/Private/Application/GV2SessionCoordinator.cpp` существующий `ContentSnapshot` не сбрасывается после остановки прежней VM. Затем устанавливается `InProgressCandidate`, вызывается document pipeline, и лишь после его успеха выполняется `ContentSnapshot = MoveTemp(Candidate)`. Однако `Source/GV2/Private/Application/GV2SessionCoordinator.h`, строки 65–68, предпочитает `ContentSnapshot`, если он непустой. Production `PrepareDocumentRequest` и runtime screen resolution используют этот getter.

**Сценарий:** успешная сессия A → повторный `StartSession` на том же coordinator без `EndSession` → initial Prepare сессии B получает A. При отличающихся screen/resource/theme данных возможны отказ по старому реестру либо применение старых значений, после которого публикуется B. Даже при одинаковом контенте нарушена identity candidate. Это вывод по конкретной ветви; динамический переход A→B с изменённой темой в этом аудите не исполнялся.

**Почему зелёный тест не опровергает находку:** `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`, строки 1492–1578, запускает одну сессию, а второй candidate строит отдельно. Второго успешного `StartSession` там нет; комментарий теста явно это оговаривает.

**Проверка для закрытия:** два настоящих успешных `StartSession` с различающимися authority values; внутри второго DocumentSink Prepare-context соответствует B, внешний snapshot не выдаёт A за готовую B. Отдельный cold-start тест этого не заменяет.

**Исход:** подтверждено как contract gap, [STATUS-014](ImplementationStatus.md).

#### PSC-AF-05 — P1 — UE-host разрушает текущую проекцию до отказоспособной проверки replacement

**Норма:** [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md): отказ private content candidate до commit-to-replace сохраняет прежнюю Ready-сессию и её опубликованное состояние.

**Подтверждение по production-коду:** `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, строки 239–262, сначала удаляет `ActiveScreen`/`ActiveGameShell`, сбрасывает reconciler и создаёт новый shell, и только затем вызывает coordinator. Его `FailReplacementAttempt(..., true)` сохраняет прежнюю Ready-сессию при ошибке candidate. Обратного восстановления прежнего shell/runtime projection в этой ветви host-а нет; recovery surface показывается только при `ApplicationState::Failed`.

**Сценарий и следствие:** Ready A → повторный публичный StartSession → ошибка Screen Registry/Image Catalog/Theme candidate. Coordinator и bindings остаются A, но её физический UI уже удалён. Проверка сохранности coordinator не доказывает сохранность продукта. Дополнительно GameShell class здесь загружается из settings до candidate, хотя его identity уже входит в snapshot. Живой отказ replacement через UI отдельно не инжектировался; вывод основан на порядке фактических вызовов.

**Проверка для закрытия:** inject content-builder failure через `UGV2RuntimeSubsystem`, сравнить до/после identity и геометрию прежнего shell, экраны, работоспособность bindings и session generation. Actual set стадий отказа должен выводиться из candidate-build stages. Проверить источник класса нового shell в успешной ветви.

**Исход:** подтверждено как contract gap, [STATUS-015](ImplementationStatus.md).

#### PSC-AF-06 — P2 — гейт Build.cs молча пропускает запрещённую зависимость в другой форме C#

**Норма:** [Build and Tooling, Presentation structural gates](../Architecture/BuildAndTooling.md#presentation-structural-gates), [ADR-0043](../ADR/0043-presentation-apply-boundary.md): graph inventory защищает Apply allowlist независимо от имени будущего authority.

**Эксперимент:** в temporary копию действующего Build.cs добавлена одна строка, вызван полный `validate_repository()` из `Tools/Testing/validate_presentation_apply_module_graph.py`. Менялась только форма массива:

```csharp
PrivateDependencyModuleNames.AddRange(new string[] { "GV2ContentCore" }); // отклонено
PrivateDependencyModuleNames.AddRange(new[] { "GV2ContentCore" });        // violations=[]
```

Baseline и штатный self-test зелёные. Парсер `extract_dependency_modules`, строки 71–84, распознаёт ограниченную форму и не сигнализирует о пропущенном dependency statement. Запрещённой зависимости в текущем Apply-модуле нет; это подтверждённая неполнота гейта, а не заявление о существующей обратной ссылке или выполненная компиляция synthetic Build.cs.

**Проверка для закрытия:** actual side — вычисленный граф UBT либо grammar с отказом на любом нераспознанном изменении dependency surface; отдельные negative fixtures для альтернативной формы, вычисляемого аргумента и нового dependency statement. Добавить только `new[]` в regex недостаточно для универсального утверждения.

**Исход:** подтверждено как contract gap, [STATUS-016](ImplementationStatus.md).

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

**Исход:** подтверждено как contract gap, [STATUS-017](ImplementationStatus.md).

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

**Открыто.** Для фиксации C++ нужен согласованный contract и runner полного `GV2`, со сверкой discovery/report sets, отсутствием skipped/not-run/in-process/error records, привязкой результата к проверенным исходникам. Ручное ожидаемое число `141` будет устаревать и перечислитель не заменяет.

#### VERIFY-AF-02 — P2 — локальный UE-runner сообщает успех при NotRun

**Эксперимент:** `Tools/MCP/run_ue_tests.py`, строки 123–126, принимает успех по `failed == 0 and total > 0`, игнорируя накопленный `failed_tests`, `skipped` и равенство passed/total. Подстановка ответа клиента `total=1, passed=0, failed=0, skipped=1`, единственная запись `state="NotRun"`, приводит к:

```text
[FAIL] GV2.Audit.Synthetic
SUCCESS: All 0/1 tests passed.
exit=0
```

Это synthetic test самого runner, не результат реального UE automation текущего аудита. Реальные отчёты разобраны и сопоставлены отдельно. Дополнительно текущий `mcp_client.py` с default `requests.Response.iter_lines()` задерживал короткий SSE-ответ до timeout; в audit-process использован `chunk_size=1`, исходник клиента не менялся.

**Открыто.** Для закрытия runner должен отвергать любой незавершённый/non-success record, несогласованность счётчиков, неполный/пустой report и несовпадение с discovery; нужны отрицательные fixtures его собственного протокола. SSE transport и завершение server task проверяются отдельно от успешности тестов.

## Ранее известные ограничения

- `STATUS-001`: replacement/preflight/cancellation/session lifecycle не завершён. Portable cold-start load существует; UE product load из этого не следует.
- `STATUS-002` и `STATUS-003`: effects и enter/exit animation paths отсутствуют. Они не препятствуют всякой gameplay-разработке, но замораживать C++ с обещанием этих возможностей нельзя.
- `STATUS-011`: обязательность данных сцены не определена; это content/schema вопрос, который следует решить до массового authoring.
- [PresentationModel](../Concepts/PresentationModel.md) всё ещё говорит об одном экране и нереализованном UI document; [CanonicalStateAndSave](../Architecture/CanonicalStateAndSave.md) одновременно содержит старую запись о неготовых migrations и отдельный раздел реализованных migrations. Валидатор links/front matter не проверяет такие смысловые противоречия. Эти описания не использованы как evidence отсутствия реально существующего кода.

## Рекомендация по фиксации C++

Обоснованное решение на текущем состоянии — **не объявлять всю C++-часть корректной и завершённой**. Продолжать экспериментальный Lua gameplay на существующем срезе можно, но это не равнозначно фиксации инфраструктуры. Новую большую перестройку этот аудит не предписывает: сначала устранить конкретные нарушения существующих contracts.

Рекомендуемая последовательность приёмки:

1. Закрыть `STATUS-013…017` и две VERIFY-находки: единственный schema authority, верный candidate при повторном запуске, сохранность прежнего UI при отказе, fail-closed lifecycle и достоверная приёмка.
2. Завершить необходимый для игры save/load путь (`STATUS-001`, `018`, `019`), проверить его через UE composition root и восстановление после отказов. Зелёная библиотечная функция не заменяет продуктовый сценарий.
3. Ограничить обещанную первую gameplay-версию: effects/animations и остальные ещё отсутствующие host capabilities либо реализуются, либо явно остаются за её пределами; отсутствие функции не выдаётся за её готовность.
4. На чистой зафиксированной ревизии повторить portable + полный UE gate и один вертикальный сценарий: старт → команда → canonical mutation/event → presentation → save → load → продолжение команды. Ожидаемые состояние, bindings и geometry проверяются независимо; transitions действительно исполняются. Для Shipping отдельно нужны cook/package и smoke запуска установленного продукта.
5. После этого фиксировать конкретную поддержанную поверхность C++/Lua и evidence baseline. Расширение capabilities, зависимостей и lifecycle впоследствии проходит тот же gate. Обещания «C++ больше никогда не меняется» и абсолютного отсутствия дефектов проверками не подтверждаются.

Аудит завершён как обследование указанного состояния. Исправление находок не выполнялось, готовность C++ не отмечалась, планы и архивы не закрывались.
