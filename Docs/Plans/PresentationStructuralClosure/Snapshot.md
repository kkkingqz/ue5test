---
title: Snapshot Tasks
status: active
version: 1.1
updated: 2026-09-07
depends_on:
  - README.md
  - PackageSet.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
---

# M2 — Snapshot

> **Материализует:** `PAH-R1/R2/R4/R5` и `D1` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md).
> **Задачи:** PSC-04…08.
> **Результат:** coordinator строит полный private candidate, публикует один immutable snapshot и передаёт semantic Prepare явный snapshot-backed context.

## Состав snapshot

`FGV2SessionContentSnapshot` — immutable C++ value/object, не `UDataAsset` и не копия repository definitions:

```text
FRepositoryReadHandle
ordered package identities
loaded Lua source set + script_set_hash
eagerly compiled UI schema set
FGV2ResolvedScreenRegistry
FGV2ResolvedImageCatalog
FGV2ResolvedUiTheme
resolved GameShell class
GC-safe asset pin set
repository_content_hash
package_set_fingerprint
presentation_hash
session_content_id
```

Absolute roots используются только candidate builders и не публикуются как runtime API. Definitions/provenance остаются в repository read handle.

## Задачи

- [ ] **PSC-04 — Построить полный session content candidate**
  - Зависимости: PSC-03.
  - Инвариант: все входы, способные изменить session behavior/presentation, замораживаются одним candidate из одного exact package set до создания Lua VM.
  - Не считается закрытием: aggregate указателей на прежних владельцев; snapshot только из четырёх известных presentation authorities; ленивый filesystem/schema fallback после `Ready`; копирование definitions/provenance.
  - Done:
    - существуют private `FGV2SessionContentCandidate` builder и immutable `FGV2SessionContentSnapshot` с полным составом выше;
    - repository handle и ordered package identities происходят из одного `FResolvedPackageSet`;
    - загруженный Lua source set и его hash принадлежат snapshot и затем передаются `FRuntimeSession` без повторного чтения дерева;
    - UI schemas компилируются eagerly; неизвестная/невалидная schema даёт typed bootstrap failure без post-Ready fallback;
    - Screen Registry, Image Catalog, Theme/styles/renderers и GameShell разрешаются candidate builder-ом;
    - `presentation_hash` покрывает resolved screens/resources/theme/GameShell asset identities; `session_content_id` канонически объединяет repository/package/script/presentation identities;
    - Unreal objects удерживаются единым GC-safe pin set на lifetime snapshot;
    - source-derived field inventory snapshot сверяется с независимой role classification и имеет negative self-test;
    - snapshot не содержит absolute roots и не копирует definitions/provenance.
  - Evidence: новые `GV2SessionContentSnapshot.*`/builder files, `GV2SessionCoordinator.*`, snapshot field inventory, lifecycle contract.

- [ ] **PSC-05 — Зафиксировать publication, replacement и recovery**
  - Зависимости: PSC-04.
  - Инвариант: partially built content/session не наблюдаем; one-VM lifecycle не нарушается обещанием сохранить уже уничтоженную VM.
  - Не считается закрытием: публикация snapshot до initial Commit; замена его полей по частям; запуск второй gameplay VM ради lifetime-теста; обещание оставить прежнюю active session после точки её teardown.
  - Done:
    - candidate snapshot используется приватно для запуска candidate VM и initial Prepare/Commit;
    - `ActiveSnapshot` становится observable только атомарно с успешным initial Commit и переходом session в `Ready`;
    - failure любого content builder до teardown replacement оставляет прежнюю active session/snapshot неизменной и уничтожает candidate целиком;
    - active snapshot и replacement content candidate сосуществуют в одном process с независимыми lifetimes, но вторая VM не запускается;
    - после teardown прежней VM ошибка новой session приводит к UE-native recovery, а не к фиктивному восстановлению старой VM;
    - cold-start recovery не требует snapshot, configured Theme, Screen Registry или Lua VM;
    - catastrophic recovery active session использует её snapshot и `LastCommittedDocument`, выполняя новый обычный Prepare, а не применяя сохранённые widget pointers;
    - failure tests покрывают каждый builder stage, publication boundary, GC lifetime и оба recovery paths через production coordinator flow.
  - Evidence: `GV2SessionCoordinator.*`, `GV2RuntimeSubsystem.*`, `GV2LayeredUiReconciler.*`, UE Automation machine report, `BootstrapAndSessionLifecycle.md`.

- [ ] **PSC-06 — Сделать snapshot владельцем и передать PrepareContext**
  - Зависимости: PSC-05.
  - Инвариант: settings/DataAssets выбирают bootstrap inputs до candidate build, но semantic runtime resolution получает authority только через `FGV2PresentationPrepareContext(snapshot)`.
  - Не считается закрытием: новый global snapshot accessor; чтение settings/DataAssets внутри semantic Prepare; session-scoped объекты с отдельными mutable owners; заявление о закрытии `GetConfiguredTheme()` до появления resolved text payload в `PSC-10`.
  - Done:
    - compiled schemas, resolved screens, image catalog, Theme/style policies и GameShell принадлежат snapshot;
    - `UGV2ScreenRegistry`/`UGV2UiTheme` остаются authoring/bootstrap inputs, но не runtime services;
    - production semantic Prepare получает snapshot только через explicit `FGV2PresentationPrepareContext`;
    - `UGV2RuntimeSubsystem` хранит coordinator и physical projection, но не отдельные mutable authority owners;
    - Text/Theme и screen/resource resolution имеют snapshot-backed Prepare entry points; их использование всеми operation kinds и удаление legacy Apply accessors являются Done `PSC-10`;
    - native recovery использует собственные минимальные значения и не является второй Theme;
    - два последовательных snapshot с разным контентом доказывают, что новая session не видит authorities предыдущей;
    - inventory мест получения `FGV2PresentationPrepareContext` выводится из parameter/field type; synthetic Prepare path без context отвергается gate/self-test.
  - Evidence: `GV2SessionCoordinator.*`, `GV2RuntimeSubsystem.*`, `GV2UiTheme.*`, `GV2ScreenRegistry.*`, `GV2ScreenFieldMaterializer.*`, PrepareContext inventory.

- [ ] **PSC-07 — Не читать ресурсы disabled packages**
  - Зависимости: PSC-06.
  - Инвариант: presentation candidate может обходить только roots из своего `FResolvedPackageSet`; исключённый контент не способен сорвать bootstrap.
  - Не считается закрытием: ignore ошибки после открытия; фильтрация после чтения либо decode; namespace-фильтр как единственная защита.
  - Done:
    - traversal, open и decode начинаются только с enabled package resource roots;
    - root list передаётся snapshot builder-ом, а Image Catalog не открывает canonical `Resources/` самостоятельно;
    - instrumented file-access test доказывает ноль opens вне set;
    - corrupt image/metadata disabled package не мешает production session start;
    - namespace/ownership validation enabled entries сохраняется как вторичная защита;
    - новый enabled package автоматически попадает в traversal через `FResolvedPackageSet`, без правки списка каталогов.
  - Evidence: `GV2ImageResourceCatalog.*`, source provider instrumentation, UE production bootstrap tests.

- [ ] **PSC-08 — Сделать screen resolution обязательным для всех placements**
  - Зависимости: PSC-06.
  - Инвариант: top-level и nested screen используют один `PrepareContext.ResolveScreen(screen_id, placement)`; класс без успешного Resolve получить нельзя.
  - Не считается закрытием: null-check configured Registry; перенос generic fallback; отдельный nested resolver; тест helper вне production Tabs path.
  - Done:
    - top-level и nested paths получают resolved descriptor только через PrepareContext и snapshot;
    - generic `UGV2ScreenWidgetBase::StaticClass()` fallback удалён;
    - missing resolver, unknown screen, forbidden placement и abstract/unloaded class дают typed Prepare failure;
    - Nested Tab negative scenario проходит реальный `FGV2TabContainerTabsPropertyConsumer`/replacement path и наблюдает отсутствие physical mutation;
    - placement cases выводятся из placement enum, screen targets — из resolved registry entries; новый enum value без policy даёт compile/test failure;
    - прежний encapsulation gate обновлён как secondary check и больше не перечисляет resolver accessor names вручную.
  - Evidence: `GV2PropertyConsumers.*`, `GV2ScreenRegistry.*`, `validate_screen_registry_entry_encapsulation.py`, UE production-path tests.

## Проверка milestone

- [ ] Snapshot содержит полный зафиксированный field set и публикуется только с `Ready`.
- [ ] Candidate failure не изменяет active snapshot; one-VM invariant сохранён.
- [ ] Snapshot является target owner, а semantic Prepare использует explicit PrepareContext; удаление legacy Apply accessors явно отложено до `PSC-10`.
- [ ] Disabled packages не открываются, nested screen не имеет bypass/fallback.
