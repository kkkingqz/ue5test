---
title: Full C++ Code Review — Archive
status: archived
version: 1.0
updated: 2026-09-14
depends_on:
  - ../ImplementationStatus.md
  - CppFoundationReadinessAudit2026-09-14.md
  - ../../Architecture/BuildAndTooling.md
---

# Полное ревью C++ части: итог раунда

> **Материализует:** исторический итог внешнего code review всех C++ модулей. Документ не является источником правил или задач: исходы находок вынесены в [сводку аудита](CppFoundationReadinessAudit2026-09-14.md), незакрытые расхождения — в [Confirmed Contract Gaps](../ImplementationStatus.md).

## Охват и метод

Ревизия `78e96f1041780f7edc62d101dbebea5391899c9f` (`main`), чистое рабочее дерево. Объём — 85 621 строка C++ без `ThirdParty/Lua54`, 348 файлов `.h`/`.cpp`, 8 модулей. Метод ревью: пять параллельных ревьюеров по областям (Runtime Core, Application Layer, UI Layer, Content Pipeline, Tests), полная portable сборка и 104/104 CTest. Динамических экспериментов ревью не проводило.

Заявленные ревью пять параллельных проверок и 104 теста не засчитаны как evidence проекта: каждая находка была независимо сверена по исходникам отдельным раундом аудита, для Value/manifest/digest — отдельным compiled probe против portable libraries. Два собственных утверждения ревью при этой сверке не подтвердились: «`GV2RuntimeCoreTests.cpp` — 10K+ строк» (фактически 2612) и «архитектура строго соответствует инвариантам» (не подтверждается при существовавших тогда `STATUS` и обнаруженном native state composition).

## Счёт

| Категория | Количество |
|---|---|
| Устранены задачами плана | 10 |
| Отклонены с наблюдаемым условием повторного открытия | 5 |
| **Всего находок** | **15** |

Пять находок породили новые contract gaps `STATUS-021…025`, закрытые и удалённые соответствующими задачами. Test hygiene и defensive guards отдельными runtime contract violations не объявлялись.

## Находки и исходы

Каждая находка разобрана блоком `CFC-AF-NN` того же номера в [сводке аудита](CppFoundationReadinessAudit2026-09-14.md), где записаны подтверждение по коду и полная формулировка исхода.

| ID | Исходная формулировка | Исход |
|---|---|---|
| `REVIEW-01` | P1 — unreferenced `UClass*` в Screen Registry → dangling pointer | Устранён задачами CFC-04A/04B; предложенный возврат runtime map на UPROPERTY DataAsset отклонён как архитектура |
| `REVIEW-02` | P1 — `AddToRoot()` без `RemoveFromRoot()` в тестах → memory leaks | Устранён задачей CFC-02A |
| `REVIEW-03` | P1 — unsafe `TObjectPtr` в non-USTRUCT → GC может собрать candidate widgets | Устранён задачей CFC-04B |
| `REVIEW-04` | P1 — replay игнорирует `Manifest.Seed` → недетерминистичный replay | Устранён задачей CFC-07A и доведён правкой по ревью M1 |
| `REVIEW-05` | P1 — hardcoded Lua state keys в C++ → architectural drift | Устранён задачей CFC-05A |
| `REVIEW-06` | P2 — hash canonicalization gap: `-0.0` через C++ API | Устранён задачей CFC-03A; заявление про NaN отклонено отдельно |
| `REVIEW-07` | P2 — hash validation inconsistency между Manifest и Digest | Устранён задачей CFC-03A |
| `REVIEW-08` | P2 — отсутствие `check(IsInGameThread())` в `PresentationApplyFacade::Apply` | **Отклонён** как подтверждённое thread violation; bounded guard добавлен CFC-04B. Условие повторного открытия — новый worker/asynchronous caller либо failed thread-affinity test |
| `REVIEW-09` | P2 — race condition в atomic save slot writes | Устранён задачей CFC-08 |
| `REVIEW-10` | P2 — global mutable state в test fixtures без teardown | Устранён задачей CFC-02A |
| `REVIEW-11` | P3 — смешение `TSharedPtr` и `std::shared_ptr` в BridgeTypes | **Отклонён** как correctness gap. Условие повторного открытия — ошибка ownership/conversion либо зависимость portable API от UE types |
| `REVIEW-12` | P3 — монолитный `GV2RuntimeCoreTests.cpp` (10K+ строк) | **Отклонён**: фактический размер 2612 строк. Условие повторного открытия — измеренный compile-time regression либо воспроизводимая проблема test ownership/discovery |
| `REVIEW-13` | P3 — неоптимальная очистка `IngressQueue::Reset()` | **Отклонён**: `Empty` в локальном UE сам реализован циклом `Pop`. Условие повторного открытия — профиль со значимой стоимостью `Reset` либо неверный queue size после reset |
| `REVIEW-14` | P3 — синхронная загрузка ассетов в `StartSession` | **Отклонён** без измерения; bootstrap contract допускает synchronous pre-VM candidate build. Условие повторного открытия — нарушение loading-time budget либо `LoadSynchronous` на пути Ready/Apply |
| `REVIEW-15` | P3 — missing indentation в `GV2RuntimeSession.cpp` | Устранён задачей CFC-05A |

## Актуальные owner contracts

- [Architecture Overview](../../Architecture/Overview.md)
- [Bootstrap and Session Lifecycle](../../Architecture/BootstrapAndSessionLifecycle.md)
- [Lua Runtime Contract](../../Architecture/LuaRuntimeContract.md)
- [Build and Tooling](../../Architecture/BuildAndTooling.md)
- [UI Document and Reconciliation](../../UI/UIDocumentAndReconciliation.md)
- [ADR-0043: Presentation Apply Boundary](../../ADR/0043-presentation-apply-boundary.md)

## Source record

`source_commit`: `2e91b1755dd6d5b1719fed3b82e4155a66c483b9` ([browse commit](https://github.com/kkkingqz/ue5test/commit/2e91b1755dd6d5b1719fed3b82e4155a66c483b9), [полный документ ревью](https://github.com/kkkingqz/ue5test/blob/2e91b1755dd6d5b1719fed3b82e4155a66c483b9/Docs/Status/CppFullCodeReview.md)).

Перед удалением path `Docs/Status/CppFullCodeReview.md` проверен через `git cat-file -e <source_commit>:<path>`, а содержимое восстановлено через `git show` и побайтно сопоставлено с рабочим файлом.
