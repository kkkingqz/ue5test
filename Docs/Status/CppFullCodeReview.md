---
title: Full C++ Code Review
status: informative
version: 1.1
updated: 2026-09-14
depends_on:
  - ImplementationStatus.md
  - AuditFindings.md
  - ../Architecture/Overview.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../Architecture/LuaRuntimeContract.md
  - ../Architecture/DependencyMap.md
  - ../Architecture/BuildAndTooling.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../ADR/0043-presentation-apply-boundary.md
---

# Полное ревью C++ части проекта

> **Показывает:** результаты полного code review всех C++ модулей проекта, новые находки сверх [текущего аудита](AuditFindings.md) и рекомендации по приоритетам исправления.
> **Не является нормативным:** правила задают owner contracts и accepted ADR; ревью фиксирует наблюдения, а не предписывает решения.
> **Исход:** каждая находка REVIEW-01…15 рассмотрена отдельно в [аудите](AuditFindings.md) блоками CFC-AF-01…15: десять устранены задачами плана C++ Foundation Closure, пять отклонены с наблюдаемым условием повторного открытия. Утверждение «архитектура строго соответствует инвариантам» аудитом не подтверждено.

## Состояние и метод

Проверена ревизия `78e96f1041780f7edc62d101dbebea5391899c9f` (HEAD → main), рабочее дерево чистое (без незакоммиченных изменений).

**Объём:** 85 621 строка C++ (без ThirdParty/Lua54), 348 файлов `.h`/`.cpp`, 8 модулей.

**Метод:** пять параллельных ревьюеров по областям (Runtime Core, Application Layer, UI Layer, Content Pipeline, Tests), каждый читал все файлы своей области, contracts и связанные ADR. Полная portable сборка и 104/104 CTest. Результаты ревьюеров верифицированы по исходникам. Динамические эксперименты (injection ошибок, runtime reproduction) в рамках этого review не проводились — используются выводы существующего [аудита](AuditFindings.md) для ранее подтверждённых gaps.

| Область | Ревьюер | Файлов | Предел вывода |
|---|---|---|---|
| Runtime Core: VM, marshalling, replay, digest, save | Runtime core reviewer | 32 | Без fuzzing, без sanitizers |
| Application: coordinator, adapters, subsystem, bridge | Application layer reviewer | 18 | Без динамического replay transitions |
| UI: widgets, reconciler, images, themes, mutations, layout | UI layer reviewer | 42 | Без rendered screenshot matrix |
| Content Pipeline: JSON5, schemas, repository, editor, authoring | Content pipeline reviewer | ~120 | Без построчного ревью всех parser ветвей |
| Tests: coverage, correctness, isolation, fixtures | Tests reviewer | 55 | Без CI runner integration checks |

## Результаты сборки и тестов

| Проверка | Результат |
|---|---|
| `cmake --build build --parallel` | success |
| `ctest --test-dir build --output-on-failure` | **104/104**, failures 0 |

## Общая оценка архитектуры

Проект строго следует заявленным инвариантам:

- Lua владеет canonical gameplay-state — C++ нигде не содержит gameplay-логики.
- C++/Lua boundary value-only — нет хранения Lua callbacks в C++.
- UI — reconstructable desired presentation, отправляет bound `command_id`, не Lua function name.
- `GV2PresentationApply` корректно изолирован от `GV2ContentCore`, `GV2RuntimeCore` ([ADR-0043](../ADR/0043-presentation-apply-boundary.md)).
- Stable ID grammar и naming conventions соблюдены последовательно.
- JSON5 парсер надёжен, без динамических аллокаций при числовом parsing.
- Портативная сборка (CMake targets) отделена от UE-зависимых модулей.

## Находки

Каждая находка получает идентификатор вида `REVIEW-NN`, severity (P1/P2/P3) и ссылку на нормативный источник. Находки, уже покрытые `AuditFindings.md`, не дублируются; при пересечении даётся ссылка.

### REVIEW-01 — P1 — unreferenced `UClass*` в Screen Registry → dangling pointer

**Норма:** UE GC contract: `UObject*` вне `UPROPERTY`/`FGCObject` невидим для GC и может быть собран.

**Файл:** `Source/GV2/Public/UI/GV2ScreenRegistry.h`, строки 215–220.

**Описание:** `FResolvedScreen` — обычная C++ структура (не `USTRUCT`), хранится в `TMap<FString, FResolvedScreen> ResolvedByScreenId` без `UPROPERTY`. Внутри — сырой `UClass* WidgetClass`, загруженный через `TSoftClassPtr::LoadSynchronous()` в `Build`. GC не видит эту ссылку и может выгрузить Blueprint-класс → dangling pointer → fatal error при создании виджета.

**Пересечение:** проблема ownership этого же registry покрыта SNAP-AF-01 / [STATUS-020](ImplementationStatus.md) (см. [AuditFindings](AuditFindings.md)); текущая находка — о GC safety самого resolved value, независимо от snapshot isolation.

**Проверка для закрытия:** `FResolvedScreen` объявлена как `USTRUCT`, `WidgetClass` — `UPROPERTY() TObjectPtr<UClass>`, `ResolvedByScreenId` — `UPROPERTY(Transient)`. Загруженный класс не собирается GC при живом registry.

---

### REVIEW-02 — P1 — `AddToRoot()` без `RemoveFromRoot()` в тестах → memory leaks

**Норма:** UE testing conventions: тестовые root-anchored объекты обязаны быть освобождены.

**Файлы:**
- `Source/GV2/Private/Tests/GV2UiPrepareCommitTests.cpp`, строка 47 (`MakeTestHostWidget()`)
- `Source/GV2/Private/Tests/GV2UiCapabilityObservabilityTests.cpp`, строки 49, 69, 178, 208, 254, 284, 313, 339, 365, 391, 415, 460

**Описание:** множественные тесты вызывают `AddToRoot()` на `UGameInstance` и никогда не вызывают `RemoveFromRoot()`. Создаёт перманентные orphaned root objects, накапливающиеся при test run. При масштабировании test suite — OOM или нестабильность Editor.

**Проверка для закрытия:** RAII обёртка (например, `FGV2ScopedGameInstance`), деструктор вызывает `RemoveFromRoot()`. Все тесты используют обёртку; после test run нет orphaned root objects от тестовых `UGameInstance`.

---

### REVIEW-03 — P1 — unsafe `TObjectPtr` в non-USTRUCT → GC может собрать candidate widgets

**Норма:** UE GC contract: `TObjectPtr` добавляет GC-ссылку только внутри `UPROPERTY`/`FGCObject`.

**Файлы:**
- `Source/GV2/Public/UI/GV2LayeredUiReconciler.h`, строки 58, 66, 81 (`FActiveScreenEntry`, `FPreparedScreenInstance`, `FPreparedReconciliationPlan`)
- `Source/GV2/Public/UI/GV2PropertyConsumers.h`, строки 396, 423, 424, 484, 517 (`FPreparedCollectionItem`, `FPreparedTabItem`, `CandidateWidgetsByKey`)
- `Source/GV2/Public/UI/GV2ScreenWidgetBase.h`, строка 13 (`FGV2ScreenFieldPlan`)

**Описание:** `TObjectPtr<>` внутри обычных C++ структур (не `UCLASS`/`USTRUCT`) не добавляет сильную GC-ссылку. Виджеты, не вставленные в UMG-иерархию (candidate widgets во время Prepare), могут быть собраны GC, оставив инвалидированные указатели.

**Примечание:** `FGV2UiPropertyMutation` в `GV2UiMutationPlan.h` корректно использует `TWeakObjectPtr` для невладеющих ссылок — это правильный паттерн.

**Проверка для закрытия:** невладеющие ссылки → `TWeakObjectPtr`; владеющие ссылки на candidate widgets → класс-потребитель наследует `FGCObject` и переопределяет `AddReferencedObjects`, либо структуры становятся `USTRUCT` с `UPROPERTY`.

---

### REVIEW-04 — P1 — replay игнорирует `Manifest.Seed` → недетерминистичный replay

**Норма:** [HeadlessSimulationContract](../Architecture/HeadlessSimulationContract.md): replay воспроизводит точную последовательность при том же seed.

**Файл:** `Source/GV2RuntimeCore/Private/GV2RunReplay.cpp`, строки 31–36.

**Описание:** `ReplayRunManifest` вызывает `Runtime.Start(1, RepositoryHandle, ...)` с захардкоженным `SessionGeneration=1`, полностью игнорируя `Manifest.Seed`. Lua VM инициализирует PRNG значением по умолчанию вместо записанного → replay diverges от оригинального run при наличии randomized gameplay.

**Подтверждение:** `FRunManifest` содержит поле `Seed` (`GV2RunManifest.h`), заполняемое при сериализации. `FRuntimeSession::Start` принимает `InSessionGeneration`, но не seed; API не предусматривает передачу seed отдельно.

**Проверка для закрытия:** расширить `Start` API для приёма seed или dispatch canonical seed setup command до replay. `ReplayRunManifest` передаёт `Manifest.Seed` в Lua VM. Replay с randomized commands даёт идентичный `StateHash`.

---

### REVIEW-05 — P1 — hardcoded Lua state keys в C++ → architectural drift

**Норма:** [Overview § Границы C++](../Architecture/Overview.md#границы-c), [INV-013](../Architecture/Invariants.md): C++ agnostic к структуре canonical state.

**Файл:** `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, строки 1438–1463.

**Описание:** `MergeStateContribution` содержит hardcoded deep merge для ключей `"instance_counters"`, `"prng"`, `"time"` внутри секции `"meta"`. C++ знает о семантике canonical state, нарушая принцип agnostic boundary.

**Проверка для закрытия:** merge logic перенесён в Lua (post-load schema или bootstrap descriptor) либо используется generic Lua-driven merge descriptor, передаваемый C++ без интерпретации ключей.

---

### REVIEW-06 — P2 — hash canonicalization gap: `-0.0` через C++ API

**Норма:** каноническое хеширование обязано быть детерминистичным для семантически равных значений.

**Файл:** `Source/GV2ContentCore/Private/CanonicalHash.cpp`, строки 40–45.

**Описание:** `std::bit_cast<uint64_t>(Value.AsNumber())` хеширует raw bits. Хотя JSON5 парсер канонизирует `-0.0` в `+0.0`, программный C++ API `FValue::MakeNumber(-0.0)` создаёт значение с отличным bitwise representation → разный hash для `+0.0` и `-0.0`. Аналогично для NaN (запрещён парсером, но не C++ API).

**Проверка для закрытия:** нормализация в `AppendCanonical`: `if (v == 0.0) v = 0.0;` + `std::isfinite` guard. Тесты подтверждают `Hash(0.0) == Hash(-0.0)` и reject NaN.

---

### REVIEW-07 — P2 — hash validation inconsistency между Manifest и Digest

**Норма:** одинаковые поля обязаны проходить одинаковую валидацию.

**Файлы:** `Source/GV2RuntimeCore/Private/GV2RunManifest.cpp`, строки 238–261; `Source/GV2RuntimeCore/Private/GV2RunDigest.cpp`.

**Описание:** `DeserializeRunManifest` проверяет `repository_content_hash` и `script_set_hash` на lowercase `a-f0-9`. `DeserializeRunDigest` пропускает эту проверку для тех же полей → одни и те же некорректные данные принимаются Digest, но отклоняются Manifest.

**Проверка для закрытия:** общая utility-функция валидации hex hash, используемая обоими парсерами.

---

### REVIEW-08 — P2 — отсутствие `check(IsInGameThread())` в `PresentationApplyFacade::Apply`

**Норма:** UE thread safety: мутация Slate/UMG виджетов допустима только из Game Thread.

**Файл:** `Source/GV2PresentationApply/Private/PresentationApplyFacade.cpp`, строка 684.

**Описание:** основная функция применения UI транзакций мутирует Slate/UMG виджеты, но не содержит assertion на Game Thread. Вызов из другого потока приведёт к undefined behavior.

**Проверка для закрытия:** `check(IsInGameThread());` в начале `Apply`.

---

### REVIEW-09 — P2 — race condition в atomic save slot writes

**Норма:** [Canonical State and Save](../Architecture/CanonicalStateAndSave.md): atomic write не допускает data corruption.

**Файл:** `Source/GV2RuntimeCore/Private/GV2SaveSlotStorage.cpp`, строка 94.

**Описание:** temporary file path `SlotPath->string() + ".tmp"` фиксирован. Два concurrent записи перезапишут один `.tmp` файл → data corruption до `rename`.

**Проверка для закрытия:** уникальный suffix (UUID/PID+timestamp) для `.tmp`. Или однопоточный доступ через документированный contract.

---

### REVIEW-10 — P2 — global mutable state в test fixtures без teardown

**Норма:** test isolation: каждый тест оставляет environment в исходном состоянии.

**Файлы:** `Source/GV2/Private/Tests/GV2ForgeryTestWidgets.cpp`, строки 13–17; `Source/GV2/Private/Tests/GV2UiCapabilityObservabilityTests.cpp`, строка 769.

**Описание:** `static EGV2ForgeryMode Mode` мутируется тестами, но не восстанавливается. Cross-test contamination при запуске в одном процессе.

**Проверка для закрытия:** scoped mutator `FScopedForgeryMode` с RAII-восстановлением в деструкторе.

---

### REVIEW-11 — P3 — смешение `TSharedPtr` и `std::shared_ptr` в BridgeTypes

**Файл:** `Source/GV2/Public/Bridge/GV2BridgeTypes.h`, строки 62–63.

**Описание:** `FGV2ScreenFieldValue` использует `TSharedPtr<const FGV2PreparedUiObject>` и `std::shared_ptr<const FCompiledUiFieldSpec>` в одной структуре. Оправдано boundary с ContentCore (без UE types), но ухудшает читаемость.

**Проверка для закрытия:** добавить комментарий с обоснованием, либо Pimpl-обёртка.

---

### REVIEW-12 — P3 — монолитный `GV2RuntimeCoreTests.cpp` (10K+ строк)

**Файл:** `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`.

**Описание:** один файл содержит более 10 000 строк тестов. Затрудняет compilation, navigation и локализацию failures.

**Проверка для закрытия:** разбить по подсистемам (`SessionTests`, `ValidationTests`, `BindingTests`).

---

### REVIEW-13 — P3 — неоптимальная очистка `IngressQueue::Reset()`

**Файл:** `Source/GV2/Private/Bridge/GV2RuntimeIngressQueue.cpp`, строки 39–42.

**Описание:** `while(Queue.Dequeue(...))` — `TQueue::Empty()` эффективнее.

---

### REVIEW-14 — P3 — синхронная загрузка ассетов в `StartSession`

**Файл:** `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, строка 251.

**Описание:** `RegistrySettings->GameShellClass.LoadSynchronous()` может вызвать hitch на Game Thread.

---

### REVIEW-15 — P3 — missing indentation в `GV2RuntimeSession.cpp`

**Файл:** `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, строки 1798–1877.

**Описание:** ~80 строк в `else` block без правильного отступа.

---

## Соотношение с предыдущим аудитом

Текущий [аудит](AuditFindings.md) зафиксировал 10 находок (PSC-AF-03..06, RUNTIME-AF-01, SAV-AF-01..02, SNAP-AF-01, VERIFY-AF-01..02), перенесённых в [STATUS-001, STATUS-011, STATUS-013..020](ImplementationStatus.md). Все запланированы к исправлению в [CFC Plan](../Plans/CppFoundationClosure/README.md), задачи CFC-01…13 + CFC-04A.

Настоящее ревью дополняет аудит **новыми** находками, не покрытыми ранее:

| Новое | Тема | Пересечение с аудитом |
|---|---|---|
| REVIEW-01 | GC safety ScreenRegistry | Частично пересекается с STATUS-020 (shared registry), но GC safety — отдельный дефект |
| REVIEW-02 | Test memory leaks | Не покрыто |
| REVIEW-03 | GC safety candidate widgets | Не покрыто |
| REVIEW-04 | Replay seed | Не покрыто |
| REVIEW-05 | Hardcoded state keys | Не покрыто |
| REVIEW-06 | Hash -0.0 | Не покрыто |
| REVIEW-07 | Hash validation parity | Не покрыто |
| REVIEW-08 | Thread safety Apply | Не покрыто |
| REVIEW-09 | Save slot race | Частично пересекается с STATUS-019 (save backup), но race — отдельный дефект |
| REVIEW-10 | Test isolation | Не покрыто |
| REVIEW-11..15 | Quality/style | Не покрыто |

## Счёт

| Severity | Количество | Описание |
|---|---|---|
| P1 (CRITICAL/HIGH) | 5 | GC safety (2), replay seed, hardcoded keys, test leaks |
| P2 (MEDIUM) | 5 | Hash gaps (2), thread safety, save race, test isolation |
| P3 (LOW) | 5 | Style, performance, maintainability |
| **Итого** | **15** | |

Из 15 находок **10 являются новыми**, не покрытыми предыдущим аудитом. 5 P1-находок требуют исправления до gameplay development; 5 P2 — до следующего milestone.

## Рекомендации по приоритетам

### Немедленно (до gameplay development)

1. **REVIEW-01**: ScreenRegistry GC safety — вероятный crash в production.
2. **REVIEW-03**: `TObjectPtr` в non-USTRUCT — latent crash во время Prepare/Commit.
3. **REVIEW-04**: Replay seed — блокирует детерминистичный replay.
4. **REVIEW-02**: Test memory leaks — destabilize CI при масштабировании.
5. **REVIEW-05**: Hardcoded state keys — architecture maintenance debt.

### До следующего milestone

6. **REVIEW-06**: Hash canonicalization — ложные content hash mismatches.
7. **REVIEW-07**: Hash validation parity — parsing inconsistency.
8. **REVIEW-08**: Game thread check — crash prevention.
9. **REVIEW-09**: Save slot race — data corruption risk.
10. **REVIEW-10**: Test isolation — flaky test risk.

### При удобном случае

11–15. Quality, style и performance improvements (REVIEW-11..15).

## Статистика по модулям

| Модуль | Файлов | LOC (approx) | Новые находки |
|---|---|---|---|
| `GV2ContentCore` | ~82 | ~24K | REVIEW-06 |
| `GV2RuntimeCore` | ~32 | ~18K | REVIEW-04, 05, 07, 09 |
| `GV2` (Application/Bridge) | ~18 | ~10K | REVIEW-11, 14 |
| `GV2` (UI) | ~42 | ~14K | REVIEW-01, 03 |
| `GV2PresentationApply` | ~53 | ~15K | REVIEW-08 |
| `GV2` (Tests) | ~55 | ~12K | REVIEW-02, 10, 12 |
| `GV2ContentHostSupport` | ~8 | ~2K | — |
| `GV2ContentAuthoring` | ~16 | ~3K | — |
| `GV2ContentEditor` | ~14 | ~3K | — |
| `GV2TestSupport` | ~5 | ~1K | REVIEW-13 |

## Граница ревью

Ревью является обследованием указанного состояния. Исправление находок не выполнялось. Абсолютная корректность C++ из этих результатов не следует. Shipping/cook/package, GPU tests, длительная нагрузка, sanitizers и fuzzing не проверялись. Findings, требующие переноса в `ImplementationStatus.md` как confirmed contract gaps, обязаны пройти validation — сверку с owner contract и code evidence — прежде чем получить STATUS-номер.
