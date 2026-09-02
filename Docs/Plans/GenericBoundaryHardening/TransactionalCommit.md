---
title: Transactional Commit Tasks
status: active
version: 1.3
updated: 2026-09-02
depends_on:
  - README.md
  - DeclaredSurface.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../Status/ImplementationStatus.md
decisions:
  - ../../ADR/0041-ui-commit-rollback-model.md
---

# M3 — Transactional Commit

> **Материализует:** `REM-02` и сверку закрытий раунда.
> **Задачи:** GBH-09…11.
> **Результат:** mid-Commit failure на переиспользуемом live UI восстанавливает предыдущую committed presentation; partial physical revision не допускается как нормальное состояние.

## Результат этапа

Это самая глубокая архитектурная недоработка раунда. Остальные находки — незавершённые абстракции или отсутствующие проверки; здесь отсутствует возможность модели.

`CommitUiHostProperties` выполняет подготовленные мутации последовательно и выходит по первому отказу. Отката нет, предыдущее физическое состояние не хранится. Для свежего кандидата, созданного off-tree, это безопасно: частично изменённый виджет ещё не опубликован. Для **переиспользуемого экземпляра** цель — уже живая предыдущая ревизия, и отказ в середине оставляет логически старую ревизию с физически частично новым интерфейсом.

Контракт при этом утверждает атомарность без оговорок.

Отдельно фиксируется, почему это не было найдено раньше: `PCC-07` закрывался инъекцией отказа, и инъекция стояла на уровне экрана, а не в середине commit переиспользуемого экземпляра. Проверка доказала случай, который проверяла.

## Задачи

- [x] **GBH-09 — Решение о механизме восстановления**
  - Выбор меняет модель, а не реализацию, поэтому принимается до кода.
  - Done: ADR фиксирует и обосновывает механизм, обеспечивающий один наблюдаемый результат: если отказ возникает после одной или нескольких live mutations, **вся предыдущая committed physical presentation восстанавливается до возврата failure и candidate revision не публикуется**. Рассматриваются как минимум: rollback stack с предыдущими projection values; shadow/staged state со свопом; rebuild из сохранённого previous prepared snapshot; иной эквивалентный transaction/recovery mechanism. Допустим отдельный путь «после первой live mutation Commit структурно не может вернуть failure» только если все fallible operations физически вынесены до неё и unexpected invariant-level failure имеет deterministic recovery к previous snapshot. Два условия этого пути связаны конъюнкцией и не подлежат раздельному исполнению: deterministic recovery к previous snapshot **не является дополнением** к выносу fallible operations, и путь, выполнивший только вынос, закрытием `REM-02` не считается. Этот путь не дешевле остальных: требуемое им восстановление само по себе является recovery mechanism, и выбирать его как облегчённый вариант — ошибка. Именно раздельное исполнение двух условий и породило `REM-02` из `PCC-07`, где инъекция была поставлена в удобную точку вместо опасной. Простое сужение нормативного контракта до допустимого partial state **запрещено как closure REM-02**. ADR отдельно рассматривает reused widgets, keyed collections, nested screens и Shell attach publication.
  - Evidence: `Docs/ADR/`, `Docs/UI/UIDocumentAndReconciliation.md`.
  - **Реализация (2026-09-02):** [`ADR-0041`](../../ADR/0041-ui-commit-rollback-model.md) принят. Решение: единый принцип "захват перед мутацией, откат в обратном порядке той же обычной Prepare/Commit-машиной" (replay предыдущего `LastCommittedProperties`, а не bespoke undo-код), применяемый на шести обнаруженных при аудите границах — property одного host'а (`CommitUiHostProperties`), host одного экрана (`CommitScreenFields`), экран одного документа (`CommitReconcile` шаг 1), элемент keyed collection (`FGV2KeyedCollectionPropertyConsumer::Commit`), вложенный экран в табе (`FGV2TabContainerTabsPropertyConsumer::CommitWithFailureInjector`) и Shell attach publication (`CommitReconcile` шаг 3). Для последних пяти из шести не существовало ранее ни одного regression-теста, воспроизводящего отказ именно на **второй** мутации переиспользуемого live host'а — `PCC-07` проверял только границу между разными экранами. Отклонены: shadow/staged дерево со свопом (несовместимо с сохранением UI-local widget state при переиспользовании), путь «Commit не может отказать после первой мутации» как самостоятельная замена (конъюнкция условий плана делает его не дешевле, GBH-01 уже даёт его как дополнение для Shell attach), сужение контракта до допустимого partial state (прямо запрещено планом). `Docs/UI/UIDocumentAndReconciliation.md` обновлён: секция «Границы транзакционности и отката» получила пункт 4 про резидуальный Commit-фазовый разрыв keyed collection, а строка про известный Shell-attach разрыв и формулировка шага 1 `CommitReconcile` теперь ссылаются на `ADR-0041` вместо описания разрыва как открытого без владельца. Regression gate для decision-задачи без кода (по прецеденту `GBH-03`): новый `Validation.validate_gbh09_rollback_decision_consistency` в `Tools/Documentation/validate_docs.py` проверяет совместное присутствие маркера `ADR-0041` в `TransactionalCommit.md`, `UIDocumentAndReconciliation.md` и самом ADR — red-on-revert продемонстрирован: временное удаление маркера из `UIDocumentAndReconciliation.md` красит `python3 Tools/Documentation/validate_docs.py`, восстановление возвращает зелёный. Реализация механизма — `GBH-10`; инъекция на опасную (вторую) точку и сверка закрытий — `GBH-11`.

- [ ] **GBH-10 — Реализация выбранного механизма**
  - Зависимости: GBH-09.
  - Done: механизм реализован для **всех** путей, где target commit может быть live/reused: несколько properties одного host, несколько fields одного Screen, reused keyed collection entries, nested screens и document/Shell publication. При failure после successful mutation A и до/на mutation B observable state A восстанавливается; `LastCommittedProperties`/ownership state, collection child identity/order, `ActiveScreens` и binding revision остаются previous revision. `GBH-01` переносит predictable attach failures в Prepare; любой оставшийся invariant-level attach failure после начала Shell publication использует тот же recovery contract, а не оставляет partial tree. Поведение подтверждено на переиспользуемом экземпляре, а не только на fresh off-tree candidate.
  - Evidence: `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`, `Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp`, `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp`.

- [ ] **GBH-11 — Инъекция в опасную точку и сверка закрытий**
  - Зависимости: GBH-10.
  - Done: test гарантированно выполняет **реальную physical mutation A**, затем инъектирует failure на mutation B того же reused live host/screen; после failure сравниваются actual renderer/control state A, `LastCommittedProperties`, child pointers/order при collection case, `ActiveScreens` и binding revision с baseline previous revision. Отдельно доказано, что прежняя screen-level injection остаётся зелёной на старом коде, а новый mid-host test — красный, то есть опасная точка действительно новая. Для каждой находки `REM-01`…`REM-07` назван regression gate и продемонстрирован red-on-revert; для deferred `REM-04` разрешён docs/status consistency gate вместо фиктивного runtime test. Закрытие класса, тронувшего несколько мест одной формы, покрывает все либо объясняет исключения; ревью переносится в `Docs/Status/Archive/` по процедуре `AGENTS.md`.
  - Evidence: отчёт change set, `Source/GV2/Private/Tests/`, `Docs/Status/Archive/`.

## Проверка milestone

- [ ] После `A.Commit == success`, `B.Commit == failure` физическое состояние A восстановлено к baseline previous revision.
- [ ] Recovery покрывает reused host, keyed collection/nested screen и document/Shell publication boundaries либо архитектурно доказано, почему конкретная boundary не может иметь post-mutation failure.
- [ ] Новый mid-host test краснеет там, где прежняя screen-level injection зелёная.
- [ ] Partial physical revision не объявлена допустимым normal contract ни в ADR, ни в `UIDocumentAndReconciliation`.
- [ ] Каждая находка раунда закрыта red-on-revert gate; для consciously deferred `REM-04` — docs/status consistency gate.
