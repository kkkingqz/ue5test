---
title: Test Suite Restructuring Plan — Archive
status: archived
version: 1.0
updated: 2026-09-15
depends_on:
  - ../../Architecture/BuildAndTooling.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../Guides/WhenToWriteCpp.md
decisions:
  - ../../ADR/0020-cpp-scope-criterion.md
  - ../../ADR/0042-presentation-authority-and-publication.md
  - ../../ADR/0046-test-content-coupling-boundary.md
---

# Test Suite Restructuring: архив плана

`source_commit: b1bfb062e60a565d4e9837f6b926be2defefcb6c`

**Цель.** Получить тестовый suite, в котором место теста определяется проверяемой границей движка, а не историей файла; contract-тест переживает переименование локации, кнопки или виджета в игровом пакете; отдельный тест читается целиком.

**Результат.** Монолит `GV2RuntimeSubsystemTests.cpp` (12 341 строка, 69 automation-тестов, 45% тестового кода модуля) растворён в шесть доменных файлов и сведён к 2 817 строкам. Правило двух категорий теста принято [ADR-0046](../../ADR/0046-test-content-coupling-boundary.md) и перенесено в [Build and Tooling](../../Architecture/BuildAndTooling.md) как нормативное. Ratchet-гейт `validate_test_content_coupling.py` заменён безусловным запретом для contract-тестов; остаток привязок в legacy-файлах зафиксирован как [`STATUS-028`](../../Status/ImplementationStatus.md).

## Этапы

| Milestone | Задачи | Результат |
|---|---|---|
| M0 — Барьер против регресса | TSR-01…02 | Перечислитель привязок из манифестов пакетов, ADR-0046, ratchet-gate с baseline и предварительным потолком размера файла |
| M1 — Перенос по файлам | TSR-03…05 | Общие хелперы в одном заголовке; шесть доменных файлов; множество test id сохранено на каждом change set |
| M2 — Отвязка от контента | TSR-06…08 | Синтетическая фикстура `core:` покрывает механику; contract-тесты отвязаны; smoke сведён к трём тестам |
| M3 — Оптимизация тестов | TSR-09…10 | Потолок размера `RunTest` 480 строк; baseline сведён к smoke-файлам и остатку под `STATUS-028`; правило в contracts |

| Задача | Название |
|---|---|
| `TSR-01` | Вывести фактические привязки тестов к контенту |
| `TSR-02` | Зафиксировать две категории теста и поставить ratchet |
| `TSR-03` | Вынести общие хелперы в один заголовок |
| `TSR-04` | Перенести save/load как пилот |
| `TSR-05` | Перенести остальные кластеры |
| `TSR-06` | Довести синтетическую фикстуру до покрытия механики |
| `TSR-07` | Перевести contract-тесты на синтетическую фикстуру |
| `TSR-08` | Свести content smoke к проверяемой поверхности |
| `TSR-09` | Раздробить тесты свыше порога |
| `TSR-10` | Обнулить baseline и перенести правило в contracts |

## Верификация приёмки (2026-09-15, ревизия `3fd9feff90fde7836bc07f746481584adc4f6bdb`)

- Portable CTest — `130/130`.
- UE automation fresh-process — `185/185`, failed/skipped `0`, `Source Diff: clean`, build fingerprint `9885392fbc30660ce353902c4ffaf177b18f4ec3404bd047ee33d93f2f41a0ed`, run `fresh-89683fe2`.
- `validate_docs.py` — 189 файлов.
- `validate_test_content_coupling.py` — зелёный; все восемь negative self-tests проходят; независимая mutation probe (`rh:location.city.market`, внесён в `GV2LayeredReconciliationTests.cpp`) отвергнута гейтом с точной причиной и откачена.
- `validate_core_decoupling.py`, `validate_cpp_foundation_closure.py` — зелёные.

## Изменение множества automation test id за весь план

Требование `Done` задачи `TSR-10` — перечислить изменения поимённо. При приёмке перечень в репозитории отсутствовал и восстановлен механически: множества сняты с `Source/**` на `d9da686~1` (до `TSR-01`) и на ревизии приёмки, затем сверены с discovery движка (`185` обнаруженных тестов). Расхождение: **9 удалено, 22 добавлено**.

Удалено `TSR-08` (`c4a0b4f`) — свёрнуто в `GV2.Runtime.Presentation.RhStartOpensLocationScreen` после того, как `TSR-07` перевёл механические утверждения на синтетическую фикстуру:

| Удалённый id | Чем покрыт |
|---|---|
| `GV2.Runtime.UI.LocationCompositeContract` | `RhStartOpensLocationScreen` (композиты `scene`/`commands`/`player_status` найдены в дереве, schema-required свойства имеют committed-значения) |
| `GV2.Runtime.UI.LocationCompositeSemantics` | там же |
| `GV2.Runtime.UI.LocationScreenTransitionContract` | там же (переход между локациями, id экранов читаются из репозитория контента) |
| `GV2.Runtime.Presentation.LocationSceneDiagnostic` | там же; диагностический тест на `AddInfo`, см. [`CFC-AF-26`](../../Status/AuditFindings.md) |

Разделено `TSR-09` (`3abc0cf`) — один id заменён несколькими, утверждения сохранены:

| Прежний id | Новые id |
|---|---|
| `GV2.UI.StandardPropertyConsumers` | `GV2.UI.Consumers.BasicWidgetsAndHosts`, `.FactoryAndPresentationAudit`, `.KeyedCollectionAndRollback`, `.ListsDropdownsAndSpans`, `.ModalAndTabContainers`, `.SceneAndCommandPanelComposite`, `.TopBarAndPlayerStatusComposite` |
| `GV2.UI.LayeredReconciliationContract` | `GV2.UI.LayeredReconciliation.BasicLifecycle`, `.ModalInteractivityAndPrepareAtomicity`, `.MultiLayerCommitFailure`, `.ReusedScreenCommitRollback` |
| `GV2.UI.PrepareCommitAndFailureInjection` | `GV2.UI.PrepareCommitGcAndGuards`, `GV2.UI.PrepareCommitPurityAndRollback` |
| `GV2.Runtime.UI.CoreRepeaterContract` | `GV2.Runtime.UI.CoreRepeaterCompositeIntegration`, `.CoreRepeaterWidgetReconciliation` |
| `GV2.Runtime.UI.NestedInstancesAndTabsContract` | `GV2.Runtime.UI.NestedScreenReconciliationContract`, `.TabContainerConsumerContract`, `.TabContainerLifecycleAndCycleContract` |
| `GV2.Runtime.UIKit.CentralThemeAndComponents` | `GV2.Runtime.UIKit.ThemeTokensAndTypographyContract`, `.WidgetThemeApplicationContract` |

Добавлено сверх разделения: `GV2.Runtime.Presentation.SyntheticMechanicalFixtureContract` (`TSR-06`, проверка самой фикстуры) и `GV2.ContentSmoke.CommonUiStyleLoads` (`TSR-08`).

`GV2.Runtime.Presentation.LocationSceneDiagnostic` был назван в `BASELINE_UE_TESTS` гейта `validate_cpp_foundation_closure.py`; его исчезновение остановило работу красным CTest до того, как замена была названа. Это сработал сам механизм: удаление именованной проверки не прошло молча.

## Что осталось открытым

- [`STATUS-028`](../../Status/ImplementationStatus.md) — 64 остаточные привязки к контенту в восьми файлах прежних планов (`DCA`, `DUC`, `UI`), зафиксированные в baseline и способные только уменьшаться. Задачи плана эти файлы не покрывали: `TSR-07` работал по файлам кластеров из `TSR-05`. Следствие для критерия выхода: переименование виджета или asset path в игровом пакете по-прежнему способно покраснить contract-тест вне `smoke_files`. Критерий выхода плана выполнен в части монолита и не выполнен в части legacy-файлов; расхождение переведено в contract gap, а не списано.
- Потолок размера файла остался предварительным значением `TSR-02` — 3306 строк, выведенных до M2 как размер наибольшего нерастворяемого файла. `TSR-09` должен был заменить его порогом по распределению, снятому после M2, и заменил только потолок `RunTest` (480 строк при фактическом максимуме 442). Практического ослабления нет: 3306 равно размеру наибольшего файла suite (`GV2RuntimeCoreTests.cpp`), то есть запас нулевой и ratchet предельно тугой; незакрытым остаётся обоснование числа, а не его строгость.

## Уроки

- **Гейт с именованным перечнем проверок окупился ровно один раз и этого хватило.** `BASELINE_UE_TESTS` не даёт удалить названную проверку молча: `TSR-07` удалил тест, CTest покраснел, и удаление получило обоснование до закрытия плана вместо того, чтобы обнаружиться через полгода.
- **Ratchet до переноса, а не после.** Порядок `TSR-02 → TSR-05` не был удобством: перенос без зафиксированного baseline легализовал бы 131 привязку в шести новых файлах, и возразить этому было бы нечем.
- **Перечень изменений множества test id нужно вести по ходу, а не восстанавливать при приёмке.** `Done` требовал его от `TSR-08` и `TSR-10`; фактически он собран здесь механическим сравнением двух ревизий. Сравнение сошлось, но это проверка постфактум: обоснование каждого удаления восстановлено чтением кода замены, а не взято из записи автора change set.
