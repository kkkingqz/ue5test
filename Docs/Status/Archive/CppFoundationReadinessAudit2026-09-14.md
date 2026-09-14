---
title: C++ Foundation Readiness Audit — Archive
status: archived
version: 1.0
updated: 2026-09-14
depends_on:
  - ../ImplementationStatus.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../Architecture/CanonicalStateAndSave.md
  - ../../Architecture/BuildAndTooling.md
---

# Аудит готовности C++: итог раунда

> **Материализует:** исторический итог раунда проверки C++-основы перед фиксацией поддержанной поверхности. Документ не является источником правил или задач: подтверждённые незакрытые расхождения живут в [Confirmed Contract Gaps](../ImplementationStatus.md), полный текст findings доступен через `source_commit`.

## Охват и метод

Раунд шёл в три захода. Первый — сплошная проверка C++-основы на рабочем дереве поверх `67058de` с семью уже существовавшими незакоммиченными правками (SHA-256 входного `git diff --binary` — `47edc48d69d7adf9b3622b22b188cf1dd7a7e6e30b08c89b3e697ae28a5c31b7`), с маршрутизацией по `Docs/README.md`, сборками, полным зарегистрированным набором automation и изолированными экспериментами. Второй — независимая сверка внешнего review snapshot ownership на `035ac04`. Третий — разбор внешнего [полного code review](CppFullCodeReview2026-09-12.md) на `78e96f1` с отдельным compiled probe против portable libraries, плюс три находки, обнаруженные уже при финальной приёмке.

Пределы вывода записаны в исходном документе и не расширялись: Shipping/package/cook, платформы кроме Linux, GPU/rendered screenshot matrix, длительная эксплуатация и нагрузочное тестирование не проверялись; параллельные анализаторы не отработали, поэтому независимым multi-agent review раунд не является. Абсолютная корректность всей C++-части из результатов не следует.

## Счёт

| Категория | Количество |
|---|---|
| Устранены задачами плана | 24 |
| Отклонены с наблюдаемым условием повторного открытия | 5 |
| Перенесены в расхождения и остались открытыми | 1 (`STATUS-027`) |
| **Всего finding blocks** | **29** |

Девять contract gaps раунда (`STATUS-013…020`, `STATUS-021…025`) были перенесены в `ImplementationStatus.md` по ходу и удалены оттуда закрывающими задачами; история удалений — в commit. Промежуточный перенос в status означал фиксацию расхождения для планирования, а не исправление кода.

## Находки и исходы

| ID | Исходная формулировка | Исход |
|---|---|---|
| `SNAP-AF-01` | P1 — snapshot разделяет mutable Screen Registry с новым candidate | Устранён задачей CFC-04A, усилен при CFC-13: authoring `UGV2ScreenRegistry` стал строго входным `const` DataAsset без мутируемого кэша; `STATUS-020` удалён |
| `PSC-AF-03` | P1 — materializer использует второй глобальный авторитет UI-схем | Устранён задачей CFC-04: процесс-глобальный кеш схем удалён, все входы materializer требуют `FGV2PresentationPrepareContext`; `STATUS-013` удалён |
| `PSC-AF-04` | P1 — повторный StartSession подготавливает первый документ через прежний snapshot | Устранён задачей CFC-06: `GetContentSnapshotForPrepare`/`InProgressCandidate` удалены, candidate передаётся явно; `STATUS-014` удалён |
| `PSC-AF-05` | P1 — UE-host разрушает текущую проекцию до отказоспособной проверки replacement | Устранён задачей CFC-06: преждевременный teardown проекции и синхронная загрузка GameShell убраны из `StartSession`; `STATUS-015` удалён |
| `PSC-AF-06` | P2 — гейт Build.cs молча пропускает запрещённую зависимость в другой форме C# | Устранён задачей CFC-03: статический regex заменён fail-closed парсером C#, неизвестная конструкция — отказ; `STATUS-016` удалён |
| `RUNTIME-AF-01` | P1 — ошибка freeze реестра не блокирует startup | Устранён задачей CFC-05: обязательная фаза sealing через `core:module.bootstrap.registry_lifecycle` с typed fault; `STATUS-017` удалён |
| `SAV-AF-01` | P1 — save/load библиотека не подключена к игровой UE-сессии | Устранён задачами CFC-09 и CFC-10: storage под `Saved/SaveGames`, safe-point save, preflight и загрузка через единый replacement; `STATUS-018` удалён |
| `SAV-AF-02` | P2 — успешная перезапись слота не сохраняет предыдущую копию | Устранён задачей CFC-08: `Current`/`Previous`, atomic publish через versioned head и immutable generations по ADR-0045; `STATUS-019` удалён |
| `VERIFY-AF-01` | P1 — CI пропускает 20 зарегистрированных тестов проекта | Устранён задачей CFC-02: весь набор `GV2` через fresh-process `run_ue_acceptance.py` со сверкой discovery inventory |
| `VERIFY-AF-02` | P2 — локальный UE-runner сообщает успех при NotRun | Устранён задачей CFC-02: fail-closed `validate_run` в `ue_test_report.py` для обоих раннеров |
| `VERIFY-AF-03` | P1 — тавтологическая проверка identity и пропуск неполного отчёта | Устранён задачей CFC-02: `source_revision`, `source_diff_hash` и `build_fingerprint` происходят из рантайма загруженного модуля |
| `CFC-AF-01` | REVIEW-01 — P1 — GC ownership registry classes | Перенесён в `STATUS-021` и устранён задачами CFC-04A/04B через compile-to-value; предложенный review возврат runtime map на UPROPERTY DataAsset как архитектура не принят |
| `CFC-AF-02` | REVIEW-02 — P2 — fixtures оставляют rooted GameInstance | Устранён задачей CFC-02A: RAII-владельцы `FScopedTestWorldContext`/`TScopedRootObject` и scope-aware гейт по actual token sites |
| `CFC-AF-03` | REVIEW-03 — P1 — off-tree candidates без traced owner | Устранён задачей CFC-04B: явный GC ownership off-tree candidates, borrowed targets через `TWeakObjectPtr`, Game Thread guard |
| `CFC-AF-04` | REVIEW-04 — P1 — seed не достигает session bootstrap | Устранён задачей CFC-07A и доведён правкой по ревью M1: первое исправление вернуло дефект в другой форме — C++-разбор container bytes, — оба парсера удалены, требования к seed разведены по формам старта, непрозрачность контейнера выводится гейтом |
| `CFC-AF-05` | REVIEW-05 — P1 — native code интерпретирует canonical state | Устранён задачей CFC-05A: сборка canonical state целиком в Lua, `MergeStateContribution`/`IsCanonicalStateSection` удалены из C++ |
| `CFC-AF-06` | REVIEW-06 — P2 — canonical zero; NaN-часть отклонена | Устранён задачей CFC-03A в части negative zero (`-0.0` → `+0.0` в конструкторе `FValue`); заявление про NaN отклонено отдельно |
| `CFC-AF-07` | REVIEW-07 — P2 — Digest принимает неканонические hash strings | Устранён задачей CFC-03A: единый предикат `IsCanonicalSha256` для всех хэш-полей Manifest и Digest |
| `CFC-AF-08` | REVIEW-08 — отсутствие локального Game Thread assertion | **Отклонён** как подтверждённое production thread violation: evidence вызова `Apply` worker-ом review не содержит. CFC-04B добавил bounded defensive guard и misuse test. Условие повторного открытия — новый worker/asynchronous caller в actual Apply call inventory либо failed thread-affinity test |
| `CFC-AF-09` | REVIEW-09 — P2 — fixed temporary slot path при concurrent writers | Устранён задачей CFC-08: один application-owned storage с exclusive process lock, `Busy` второму владельцу; unique filename не использовался как замена протоколу публикации |
| `CFC-AF-10` | REVIEW-10 — P2 — forgery mode не восстанавливается | Устранён задачей CFC-02A: RAII-класс `FScopedForgeryMode`, прямой вызов мутации закрыт статическим гейтом |
| `CFC-AF-11` | REVIEW-11 — смешение shared pointer families | **Отклонён** как correctness gap: разные pointer families отражают модульную границу между UE prepared value и portable compiled schema. Условие повторного открытия — ошибка ownership/conversion либо изменение portable API, создающее реальную зависимость от UE types |
| `CFC-AF-12` | REVIEW-12 — размер RuntimeCoreTests | **Отклонён** как foundation blocker: `wc -l` на проверенной ревизии даёт 2612 строк, не 10K+; ни compilation regression, ни failure-localization metric не приведены. Условие повторного открытия — измеренный compile-time regression этого translation unit либо воспроизводимая проблема test ownership/discovery |
| `CFC-AF-13` | REVIEW-13 — Queue.Empty как оптимизация | **Отклонён** как доказанная проблема: `Reset` корректно dequeue-ит и сбрасывает `QueueSize`, а локальный `Containers/Queue.h` реализует `Empty` циклом `Pop`. Условие повторного открытия — профиль, показывающий существенную стоимость `Reset`, либо неверный queue size после reset |
| `CFC-AF-14` | REVIEW-14 — synchronous asset load на старте | **Отклонён** как самостоятельный performance defect без измерения: bootstrap contract прямо допускает synchronous pre-VM candidate build. Условие повторного открытия — измеренное нарушение принятого loading-time budget либо найденный `LoadSynchronous` на пути Ready/Apply |
| `CFC-AF-15` | REVIEW-15 — indentation lifecycle block | Устранён задачей CFC-05A в рамках переноса State Composition |
| `CFC-AF-16` | CFC-13 — P1 — `FRuntimePhaseResult::Fault` не достигал production callback | Устранён задачей CFC-13: каждый отказ фазы передаётся callback как `Fault` после восстановления Lua stack |
| `CFC-AF-17` | CFC-13 — P1 — Lua error обходил деструкторы C++ RAII | Устранён задачей CFC-13: работа с repository вынесена во внутреннюю функцию, `lua_error` поднимается только из тривиального trampoline |
| `CFC-AF-18` | CFC-13 — P1 — milestone plan не входил в actual enumerator | Устранён задачей CFC-13: milestone checkbox'ы плана стали частью actual enumerator приёмочного гейта |

## Ранее известные ограничения и их исход

- `STATUS-001` закрыт задачами CFC-07 и CFC-10 после production replacement, preflight/cancellation и сценария `load-another-save`.
- `STATUS-011` закрыт задачей CFC-11: обязательная scene surface и typed отказ закреплены fixtures схемы v2.
- `STATUS-002` и `STATUS-003` (one-shot effects и enter/exit animations) остались открытыми сознательно: они не блокируют синхронный gameplay-срез, но запрещают называть весь presentation contract реализованным.
- `STATUS-026` (модуль изоляции реестров в графе production-сессии) открыт с условием закрытия, требующим отдельного ADR.
- Смысловые противоречия между документами валидатор links/front matter не ловит. На момент архивации из найденных осталось одно устаревшее утверждение в `PresentationModel`, снятое commit-ом `2e91b17`.
- Обязательный job `Unreal GV2 Acceptance` ни разу не исполнялся на `origin`: открытый пункт evidence CFC-02 перенесён в [`STATUS-027`](../ImplementationStatus.md).

## Решение раунда

C++/Lua foundation принята задачей CFC-13 в ограниченной проверенной поверхности Linux Editor/Development. Это не обещание абсолютной корректности всей C++-части и не заморозка C++ навсегда: новое native API обязано в том же change set получить обоснование по `INV-013`, production consumer, negative fixture и actual enumerator. Обычные Command, Validator, Event, gameplay Service, migration и presentation semantics остаются Lua-owned.

Итоговая перепроверка на ревизии `02cb996b4b905f383b724aa099b1b9324cebd2f5`: Release CTest 128/128, ASan+UBSan CTest 128/128, UBT `GV2Editor Linux Development` — success, fresh-process UE 173/173 при `source_diff_hash=clean`. Полный UE report содержит 230 warnings и warning-free evidence не является. Удалённый CI за эту ревизию не засчитан.

## Актуальные owner contracts

- [Bootstrap and Session Lifecycle](../../Architecture/BootstrapAndSessionLifecycle.md)
- [Runtime Facade and Registries](../../Architecture/RuntimeFacadeAndRegistries.md)
- [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md)
- [Build and Tooling](../../Architecture/BuildAndTooling.md)
- [ADR-0021: Opaque Save Container](../../ADR/0021-opaque-save-container.md)
- [ADR-0044: Session Replacement and Registry Sealing](../../ADR/0044-session-replacement-and-registry-sealing.md)
- [ADR-0045: Atomic Save Slot Generation Publication](../../ADR/0045-atomic-save-slot-generation-publication.md)

## Source record

`source_commit`: `2e91b1755dd6d5b1719fed3b82e4155a66c483b9` ([browse commit](https://github.com/kkkingqz/ue5test/commit/2e91b1755dd6d5b1719fed3b82e4155a66c483b9), [полный документ findings](https://github.com/kkkingqz/ue5test/blob/2e91b1755dd6d5b1719fed3b82e4155a66c483b9/Docs/Status/AuditFindings.md)).

Перед удалением path `Docs/Status/AuditFindings.md` проверен через `git cat-file -e <source_commit>:<path>`, а содержимое восстановлено через `git show` и побайтно сопоставлено с рабочим файлом.
