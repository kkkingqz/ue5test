---
title: C++ Foundation Re-Review Findings
status: informative
version: 1.0
updated: 2026-09-14
depends_on:
  - ImplementationStatus.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../Architecture/RuntimeFacadeAndRegistries.md
  - ../Architecture/BuildAndTooling.md
---

# Повторное ревью C++ foundation: находки и их проверка

> **Показывает:** внешнее повторное ревью session/presentation boundaries после закрытия плана C++ Foundation Closure и результат проверки каждого его утверждения по коду.
> **Не является нормативным:** правила задают owner contracts и accepted ADR. Формулировки ревью не являются нормой; нормой является contract, на который они ссылаются.
> **Исход:** раунд открыт. Пять заявленных находок подтверждены, две из них с существенным уточнением; сверх ревью найдены две сопутствующие проблемы. Ни одна находка пока не получила исход.

## Состояние и метод

Источник — внешнее ревью от 2026-09-14, выполненное на `5fe3c824ced129540a324b8d4891da6b601e151d`. Проверка выполнена на `e7d675487444a859ff54a20015b7a6e36ae2ed72`; все пять production-файлов, к которым относятся находки, между этими ревизиями не изменялись (`git diff 5fe3c82 HEAD -- <path>` пуст для каждого), поэтому результат относится к обеим ревизиям.

Метод: чтение production-путей от публичной точки входа до места, где заявленный эффект наступает, с фиксацией `file:line`; сверка заявленного нарушения с текстом owner contract, а не с формулировкой ревью; поиск уже существующего механизма, который заявленную проблему решает или должен решать.

**Чего проверка не делала.** Ни одна находка не воспроизведена исполнением: UE-сборка и automation-прогон не запускались, потому что в дереве шла параллельная работа. Поэтому ниже сказано «подтверждено по коду», а не «воспроизведено». Для `LIFE-R1` путь от публичного вызова до остановки VM безусловный и не содержит развилок, для остальных находок утверждение структурное — но это всё ещё чтение, и обязательные regression-тесты из ревью не написаны.

Утверждения ревью о закрытых пунктах прошлого цикла отдельно не перепроверялись сплошным чтением: они совпадают с [архивной сводкой раунда](Archive/CppFoundationReadinessAudit2026-09-14.md), где каждый соответствующий finding закрыт задачей и проверен мутацией. Перепроверка выполнена точечно — см. раздел ниже.

## Счёт

| Категория | Количество |
|---|---|
| Заявлено ревью | 5 |
| Подтверждено по коду | 5 |
| Из них с уточнением severity или механизма | 2 |
| Найдено сверх ревью | 2 |

Ни одно утверждение ревью не оказалось ложным. Два утверждения оказались точнее или шире, чем заявлено, и одно предложенное исправление не принимается в предложенном виде — см. `CFC-AF-19`.

## CppFoundationClosure — повторное ревью

#### CFC-AF-19 — LIFE-R1 — P1 — невалидный `RequestSession` уничтожает работающую Ready-сессию

**Источник:** «Публичный `RequestSession` проверяет descriptor до передачи request в нормальный transition protocol; `FailBootstrap()` переводит ошибку в `FailRuntime()`, и внешний плохой replacement request B способен сломать уже работающую session A до `commit-to-replace`».

**Подтверждено по коду.** `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp:232-244`: `RequestSession` — `UFUNCTION(BlueprintCallable)` — вызывает `Descriptor.IsValid()` и при отказе выполняет `Coordinator->FailBootstrap(TEXT("InvalidSessionDescriptor"), ...)` и `return 0`. `GV2SessionCoordinator.cpp:876-882`: `FailBootstrap` — тонкая обёртка над `FailRuntime`. `GV2SessionCoordinator.cpp:1366-1410`: `FailRuntime` безусловно снимает `Status.bIsReady`, завершает `BindingRegistry`, сбрасывает `IngressQueue`, останавливает `RuntimeSession`, переводит application/session state в `Failed` и обнуляет `PinnedRepository`, `ContentSnapshot`, `RepositoryVersion`. Проверки «есть ли живая Ready-сессия» на этом пути нет. Соседняя ветка `RepositoryNotReady` (`GV2RuntimeSubsystem.cpp:246-256`) имеет ту же форму.

Это нарушает [replacement protocol](../Architecture/BootstrapAndSessionLifecycle.md#session-replacement-protocol) и [ADR-0044](../ADR/0044-session-replacement-and-registry-sealing.md): до `commit-to-replace` отказ кандидата B не вправе менять A.

**Уточнение — механизм уже существует и обходится.** Ревью предлагает перестроить протокол: выделять operation id, ставить операцию, валидировать внутри. Это уже сделано и работает. `GV2SessionCoordinator.cpp:492-509`: `ExecuteSessionStart` запоминает `bHadPriorReadySession = Status.bIsReady`, валидирует тот же descriptor и при отказе вызывает `FailReplacementAttempt({"InvalidSessionDescriptor", ...}, bHadPriorReadySession)` плюс `RecordOutcome(Op.OperationId, Failed)`. `GV2SessionCoordinator.cpp:1340-1358`: `FailReplacementAttempt` при `bHadPriorReadySession == true` **не трогает ничего** — комментарий перечисляет `Status`/`PinnedRepository`/`BindingRegistry`/`RuntimeSession`/`ContentSnapshot` поимённо.

Существующий тест `GV2.Runtime.Session.UninitializedDescriptorSeedRejected` (`Source/GV2/Private/Tests/GV2SessionSeedReplayTests.cpp:303-332`) вызывает **coordinator**-уровневый `RequestSession` с пустым `SeedHex` и получает ненулевой operation id с исходом `Failed` — то есть проверяет корректный путь и потому не видит дефекта, который лежит уровнем выше.

Следовательно, правильное исправление — убрать дублирующую предварительную валидацию из `UGV2RuntimeSubsystem::RequestSession` (обе ветки) и позволить операции дойти до coordinator, а не строить новый протокол. Побочно это устраняет и второй дефект, названный ревью: возврат `0` вместо operation id.

**Область действия.** Продуктовые пути `RequestLoad` и `StartSession` при живой Ready-сессии сюда не попадают: `RequestLoad` при `bIsReady` уходит в `Coordinator->RequestLoad` раньше (`GV2RuntimeSubsystem.cpp:322-324`), а `StartSession` строит descriptor сам с валидным seed. Достижимость дефекта обеспечивает публичность `RequestSession` для Blueprint и любого native caller, а не внутренний сценарий.

**Исход:** открыт.

#### CFC-AF-20 — PKG-SNAP-R1 — P1 — `package.json5` читается второй раз после фиксации package set

**Источник:** «`FResolvedPackageSource` хранит Root/Descriptor/CanonicalManifestHash, но Screen Registry позднее получает `ue_content_roots` повторным чтением `package.json5`; один semantic manifest имеет два момента чтения».

**Подтверждено по коду.** `Source/GV2ContentHostSupport/Public/GV2ContentHostSupport/PackageDiscovery.h:101-106`: состав `FResolvedPackageSource` — ровно `Root`, `Descriptor`, `CanonicalManifestHash`. `Source/GV2/Private/UI/GV2ScreenRegistry.cpp:41-80`: `ReadUeContentRootsForPackage` открывает `<root>/package.json5` через `FFileHelper::LoadFileToString`, парсит `ParseJson5Document` и читает поле `ue_content_roots`. Вызывается из `ResolveContentRootOwnershipFromGameData` (`:160-180`), который вызывается из `CompileResolvedRegistry` (`:247, 269`).

**Уточнение — окно шире, чем описано в ревью, и шире, чем утверждает собственный комментарий кода.** Ревью описывает `t0 = ResolvePackageSet()`, `t1 = FGV2SessionContentCandidate::Build()`. Комментарии в `GV2ScreenRegistry.cpp:38-40` и `:158-159` при этом до сих пор утверждают, что чтение происходит «only called from Build(), only called from LoadScreenRegistry(), only called from Initialize(), before any session exists». Это больше не так: `CompileResolvedRegistry` вызывается из `GV2SessionContentSnapshot.cpp:120`, то есть при построении candidate **каждой** сессии, включая каждый replacement. `ResolvedPackageSet` при этом захватывается один раз в `Initialize`. Окно между захватом идентичности и чтением файла — не миллисекунды внутри `Initialize`, а всё время жизни приложения до очередного старта сессии.

Расхождение наблюдаемо без гонки: достаточно отредактировать `ue_content_roots` в работающем приложении и запустить новую сессию — `package_set_fingerprint` останется прежним, а ownership контентных корней будет построен по новому содержимому файла. При этом `ue_content_roots` входит в `CanonicalManifestHash` (`PackageDiscovery.h:95-100`, negative case `case13_ue_content_roots_did_not_change_fingerprint` в `PackageDiscoveryAndOrderConformance.cpp:826-851`) — то есть поле признано семантическим, но проверка его неизменности на втором чтении отсутствует.

**Исход:** открыт.

#### CFC-AF-21 — SNAP-R2 — P1 — Theme в snapshot остаётся mutable authoring `UObject`

**Источник:** «Для Screen Registry используется compile-to-value, для Theme — strong pointer на `UGV2UiTheme` DataAsset; semantic state не был реально frozen, а изменение asset in-place не меняет `PresentationHash`/`SessionContentId`».

**Подтверждено по коду.** `Source/GV2/Private/Application/GV2SessionContentSnapshot.h:27-36`: `FGV2ResolvedUiTheme` содержит `TStrongObjectPtr<UGV2UiTheme> Theme` и `TStrongObjectPtr<UGV2UiTheme> FallbackTheme` — указатели на authoring-ассеты, не скомпилированные значения; имя типа говорит «resolved», содержимое — нет. Для сравнения, `FGV2ResolvedScreenRegistry` в том же файле хранит строки-значения. Чтение полей темы на фазе Prepare: `GV2TextPipeline.cpp:251,258`, `GV2CentralStylePreparer.cpp:76,389`, `GV2PropertyConsumers.cpp:1798-1800`.

**Уточнение — сильнее, чем заявлено.** Ревью говорит, что `PresentationHash` «не изменяется» после правки темы. Фактически содержимое темы в идентичность не входит вовсе: `GV2SessionContentSnapshot.cpp:211` кладёт в presentation-хэш `ResolvedTheme->GetPathName()` — путь ассета. Соседние ресурсы в том же хэше участвуют содержательно (`resource_id`, путь текстуры, `render_mode`, `:200-208`), а Screen Registry скомпилирован в значения. То есть дело не в моменте вычисления хэша, а в том, что ни одна семантическая величина темы никогда не участвует ни в `PresentationHash`, ни в `SessionContentId` (`:236-241`). Две сессии с разными значениями темы неотличимы по идентичности содержимого.

**Исход:** открыт.

#### CFC-AF-22 — LIFE-R2 — P2 — `Failed` operation не несёт typed fault

**Источник:** «Lifecycle contract требует, чтобы `Failed` нёс typed fault; реализация хранит только `TMap<uint64, ESessionOperationOutcome>`, и наружу все причины выглядят одинаково».

**Подтверждено по коду и по contract.** [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md) прямо требует: «Terminal operation outcome имеет закрытое множество `Completed | Failed | Cancelled | Superseded`. `Failed` несёт typed fault». Реализация: `Source/GV2/Private/Application/GV2SessionTransition.h:129` — `TMap<uint64, ESessionOperationOutcome> OperationOutcomes`; `GV2SessionTransition.cpp:527-534` `GetOutcome` возвращает только enum; публичный `UGV2RuntimeSubsystem::GetSessionOperationOutcome` (`GV2RuntimeSubsystem.cpp:347-360`) — тоже. Fault формируется и передаётся в `FailReplacementAttempt`/`FailRuntime`, где уходит в лог, но не связывается с operation id.

Это прямое расхождение contract и реализации, а не пожелание к API.

**Исход:** открыт.

#### CFC-AF-23 — LIFE-R3 — P3 — terminal operation outcomes не имеют bounded retention

**Источник:** «Coordinator живёт весь `GameInstance`, operation IDs выдаются в том числе save/load requests; если terminal records не очищаются, long-running process накапливает их без ограничения».

**Подтверждено по коду, и сильнее заявленного.** `GV2SessionTransition.cpp:547-553`: `RecordOutcome` только добавляет в `OperationOutcomes`. Единственная точка очистки — `FGV2SessionTransitionPolicy::Reset()` (`:560-566`), и **у неё нет ни одного вызывающего** во всём репозитории, включая тесты. То есть это не «policy не сформулирована», а «единственный механизм очистки мёртв»: за время жизни `GameInstance` карта только растёт, по записи на каждый save/load/session request.

Практический вес остаётся низким — запись это `uint64` плюс enum, и заметный рост требует очень длинной сессии, — но `Reset()` без вызывающих является и самостоятельным дефектом: код, выглядящий как реализованная policy, ею не является.

**Исход:** открыт.

## Находки сверх ревью

#### CFC-AF-24 — устаревшие комментарии о месте вызова в `GV2ScreenRegistry.cpp`

`GV2ScreenRegistry.cpp:38-40` и `:158-159` утверждают «only called from `Build()`, only called from `LoadScreenRegistry()`, only called from `Initialize()`, before any session exists». Ни `Build()`, ни `LoadScreenRegistry()` больше не являются вызывающими: путь идёт через `CompileResolvedRegistry` из candidate build каждой сессии (`GV2SessionContentSnapshot.cpp:120`). Комментарий описывает свойство, на котором держится оценка риска `CFC-AF-20`, и это свойство утрачено.

**Исход:** открыт.

#### CFC-AF-25 — `FGV2SessionTransitionPolicy::Reset()` не имеет вызывающих

Выделено из `CFC-AF-23` отдельно, потому что это разные решения: одно — какой должна быть retention policy, другое — что в коде уже есть неиспользуемый метод очистки, который создаёт впечатление существующей policy.

**Исход:** открыт.

## Проверка утверждений ревью о закрытых пунктах

Ревью отдельно утверждает, что четыре проблемы прошлого цикла закрыты, и что переоткрывать план целиком не требуется. Точечная сверка:

| Утверждение ревью | Результат сверки |
|---|---|
| Screen Registry переведён в compile-to-value | Подтверждается: `UGV2ScreenRegistry::CompileResolvedRegistry` (`GV2ScreenRegistry.cpp:247`) — `const`-метод, результат `FGV2ResolvedScreenRegistry` — значение. Совпадает с исходом `SNAP-AF-01`/`CFC-AF-01` архивного раунда |
| UI Schema authority идёт через snapshot и `FGV2PresentationPrepareContext` | Подтверждается косвенно: `GetTheme()`/`GetSchemaCache()` доступны только через `FGV2PresentationPrepareContext` (`GV2SessionContentSnapshot.h:143`), процесс-глобального кеша в дереве нет. Соответствует исходу `PSC-AF-03` |
| Candidate передаётся явно, без ambient выбора A/B | Подтверждается: `GetContentSnapshotForPrepare` в дереве отсутствует; см. исход `PSC-AF-04` |
| Save/load подключён к production lifecycle | Подтверждается: см. `CFC-09`/`CFC-10` и записи приёмки M2 |

Таблица открытых gaps в ревью (`STATUS-002`, `STATUS-003`, `STATUS-026`, `STATUS-027`) совпадает с [Confirmed Contract Gaps](ImplementationStatus.md) по составу и смыслу.

Вывод ревью «переоткрывать C++ Foundation Closure целиком не требуется» не оспаривается: ни одна из находок не отменяет принятую поверхность, все пять лежат внутри неё и являются дефектами реализации либо незавершённой частью уже принятого решения.

## Пределы проверки

- Ни одна находка не воспроизведена исполнением; обязательные regression-тесты, названные ревью, не написаны.
- Severity в исходных формулировках сохранена как заявленная и отдельно не пересматривалась, кроме случаев, где уточнён механизм.
- Ревью читало `5fe3c82`; работа параллельной сессии над тестовым suite (`TSR-07`) на выводы не влияет — все затронутые файлы production-уровня.
- Полный UE automation, portable CTest и sanitizer-прогон в рамках этой проверки не запускались.
