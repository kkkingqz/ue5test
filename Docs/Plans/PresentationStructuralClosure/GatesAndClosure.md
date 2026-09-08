---
title: Structural Gates and Closure Tasks
status: active
version: 1.2
updated: 2026-09-08
depends_on:
  - README.md
  - ApplyBoundary.md
  - ../../Status/AuditFindings.md
---

# M5 — Structural Gates and Closure

> **Материализует:** `PAH-R7`, `D4` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md) и доказуемое закрытие `PAH-R1…R7`.
> **Задачи:** PSC-13…14.
> **Результат:** каждое универсальное утверждение имеет механический actual-set и production-path oracle; active records подготовлены к обязательной post-completion архивации без потери задач.

## Матрица первичных перечислителей

| Утверждение | Actual-set enumerator | Независимый oracle |
|---|---|---|
| Где создаётся package set | declarations, возвращающие `FResolvedPackageSet`, и их call sites | разрешённые host bootstrap layers |
| Какие модули связаны | все UBT `*.Build.cs` плюс все CMake target/source declarations | dependency allowlist/denylist из contract |
| Какие операции применяются | exhaustive operation enum/variant | независимая role/behavior classification |
| Какие central-style targets мутируются | реализации value-only target interface и все его production call sites | только prepared central-style operation внутри transaction façade; отдельно design-time-only branch |
| Что может нести payload | recursive fields из public declarations | forbidden capability categories |
| Какие Apply API экспортированы | declarations всего `GV2PresentationApply/Public` | одна transaction façade плюс DTO/result/widget roles |
| Какие Widget Blueprint мигрируются | Asset Registry inheritance/reference closure | ноль old class paths после clean reload |

Сканы конкретных имён авторитетов остаются diagnostic/reminder checks. Они не доказывают полноту authority set.

## Задачи

- [ ] **PSC-13 — Собрать structural gates и adversarial production scenarios**
  - Зависимости: PSC-12.
  - Инвариант: универсальное утверждение закрывается только enumerator-ом фактического множества и независимым expected oracle; новый элемент не требует помнить имя в старом тесте.
  - Не считается закрытием: расширение regex-list; `Commit*` prefix как actual set; ручной список source files; self-test без production-path test; UBT graph без CMake/Headless graph.
  - Done:
    - каждый enumerator из таблицы реализован и имеет synthetic negative self-test;
    - module graph отвергает `GV2PresentationApply → GV2/GV2ContentHostSupport/DeveloperSettings/AssetRegistry/ImageCore/authoring`;
    - Headless/CMake graph отвергает UE/UMG/CommonUI/`GV2PresentationApply` source/link edge;
    - exported Apply API inventory отвергает вторую transaction façade или physical mutation entry point вне явно классифицированных widget/lifecycle roles;
    - operation enum/variant без `default` даёт compiler error для нового необработанного kind;
    - central-style implementation/call-site inventory отвергает новый target без prepared operation, runtime `NativePreConstruct` application и physical helper call вне transaction façade;
    - recursive payload inventory отвергает soft reference, resolver/context/callback/service handle, включая nested members;
    - forbidden-capability scan перечисляет actual module source tree автоматически и отвергает synchronous load/settings/filesystem imports/calls; документирована граница, что он не является полной классификацией всех будущих UE API;
    - package-set factory/call inventory отвергает downstream rediscovery независимо от имени helper;
    - configured-accessor symbol gate отвергает declaration, definition или call site `GetConfiguredTheme()`/`GetConfiguredRegistry()`; отдельный production call-site inventory разрешает `GetCoreMinimalTheme()` только recovery surface;
    - mandatory production scenarios проходят: Theme resolve only in Prepare; central style applied as prepared transaction operation; design-time preview does not read runtime authority; nested Tab resolution failure; different Editor set shared by all consumers; corrupt disabled resource unopened; snapshot replacement lifetime; failed candidate preserves active before teardown; cold-start recovery uses only core-minimal values; catastrophic recovery uses the pinned snapshot through normal Prepare/Apply; `ue_content_roots` fingerprint separation; initial screen from snapshot;
    - runtime authority counter показывает Prepare accesses и ноль accesses вокруг Apply для каждого operation kind, включая central style;
    - для каждого `PAH-R1…R7` записано, какой gate краснеет при revert, и revert/synthetic mutation действительно демонстрирует failure;
    - contracts описывают назначение и ограничения каждого gate; source scans явно названы secondary там, где множество capabilities открыто.
  - Evidence: `Tools/Testing/`, `Source/GV2PresentationApply/`, portable conformance, UE Automation tests/report, обновлённые owner contracts.

- [ ] **PSC-14 — Выполнить независимую сверку и подготовить закрытие**
  - Зависимости: PSC-13.
  - Инвариант: закрывается класс дефекта, а подтверждённое расхождение не исчезает только потому, что audit становится архивом.
  - Не считается закрытием: ссылка на task вместо red-on-revert; prefix subset UE tests; число тестов из grep лога; изменение golden без replay; преждевременное удаление active audit/plan.
  - Done:
    - `PAH-R1…R7` построчно сверены с Done/Evidence, каждый finding содержит допустимый исход;
    - отдельно записано, чем гарантирована полнота authority/apply/package/payload sets и чего каждый gate по построению не видит;
    - ни одно закрытие не сужает `ADR-0041/0042/0043`; сохранившийся contract gap перенесён в `ImplementationStatus.md` до архивации;
    - portable дерево заново configured/built; полный CTest, `gv2-headless --self-test`, `gv2-headless --check-scripts`, content smoke и docs validation проходят;
    - Headless golden воспроизведён из записанного manifest, machine-readable digest совпадает; presentation-only изменения не требуют golden update;
    - полный `Automation RunTests GV2` проходит, а executed/pass/fail/skip counts читаются из машинного отчёта;
    - `AuditFindings.md` сохраняет каждый finding с допустимым исходом; выжившие gaps уже перенесены в `ImplementationStatus.md`;
    - proposal готов к состоянию `implemented`: все его требования сопоставлены с прошедшим evidence;
    - каждый task и milestone плана может быть отмечен `[x]` без незакрытого требования; archive summaries содержательно подготовлены, но active records ещё не удалены;
    - `STATUS-001…003` и `STATUS-011` не меняются без отдельного evidence: они вне scope этого плана.
  - Evidence: machine reports, resolved `AuditFindings.md`, полностью отмеченный active plan и итоговое сопоставление Proposal → implementation.

## Архивация после выполнения PSC-14

Архивация — lifecycle завершённого плана, а не требование ещё не завершённой checkbox-задачи:

1. Создать первый commit, в котором полный `AuditFindings.md` содержит исходы, а полный plan — все tasks/milestones `[x]`. Его полный hash является общим `source_commit` audit и plan archives.
2. До удаления проверить каждый plan path и audit path через `git cat-file -e <source_commit>:<path>`; восстановить representative plan file и audit через `git show`.
3. Вторым commit создать плоские audit/plan summaries с каждым finding/task ID ровно один раз, полным hash и repository web links; удалить active audit и plan directory; обновить оба archive indexes.
4. В том же втором commit поставить Proposal `proposal_state: implemented`, `status: archived`, перенести его в `Proposals/Archive/` и обновить три proposal indexes/links. Proposal не имеет отдельной двухкоммитной процедуры.

## Проверка milestone

- [ ] Ни одно первичное доказательство не основано на prefix/ручном списке имён.
- [ ] UBT и CMake/Headless graphs проверены независимо.
- [ ] Каждый finding имеет red-on-revert evidence через production path.
- [ ] Active records готовы к первому closure commit; последующая архивная процедура записана без self-referential checkbox.
