---
title: Instrument Reach Tasks
status: active
version: 1.1
updated: 2026-08-31
depends_on:
  - README.md
  - SilentLoss.md
  - SwallowedFailure.md
  - ../../UI/WidgetRegistry.md
---

# M3 — Instrument Reach

> **Материализует:** расширение охвата harness и `UPP-R7` в части документации.
> **Задачи:** PCC-10…12.
> **Результат:** области, где до сих пор находились дефекты, попадают в поле зрения инструмента.

## Результат этапа

Пять находок P1 обнаружены при полностью зелёной верификации. Это не случайность: каждая лежит там, куда harness не наведён по построению.

Harness перебирает объявленные capability существующих виджетов верхнего уровня. Он не заходит внутрь элементов коллекций (`UPP-R1`), не видит вид, объявленный без реализации (`AddObject`), и ничего не знает о том, потребляется ли результат `Commit` (`UPP-R2`). Три слепые зоны — три находки.

Этап выполняется последним, потому что его гейты фиксируют результат M1 и M2 и обязаны краснеть на их откате.

## Задачи

- [x] **PCC-10 — Harness заходит внутрь элементов коллекций**
  - Зависимости: PCC-03.
  - Сейчас sweep перебирает capability хоста, а `CollectionHost` намеренно пропускает: обоснование в коде отсылает к тому, что коллекции доказываются задачами миграции композитов. `UPP-R1` показал, что этого недостаточно.
  - Done: для каждой capability вида коллекции harness инстанцирует entry-виджет и перебирает **его** capability тем же правилом различимых состояний; capability элемента, не меняющая наблюдаемого состояния, краснит сборку; harness краснеет в трёх сценариях подделки, проверенных явно, — consumer элемента заменён на no-op, renderer элемента отвязан, capability элемента объявлена без реализации; охват sweep выводится из списка всех `IGV2UiPropertyHost` и всех их коллекций, а не перечисляется вручную.
  - Evidence: `Source/GV2/Private/UI/GV2UiCapabilityObservability.cpp`, `Source/GV2/Private/Tests/GV2UiCapabilityObservabilityTests.cpp`.
  - **Реализация (2026-08-31):**
    - `RunUiCapabilityObservabilityHarness`: для `Cap.TargetType == CollectionHost` — инстанцирует `Cap.EntryWidgetClass` через `CreateWidget`, кастует к `IGV2UiPropertyHost`, рекурсивно вызывает то же правило на его собственном дереве capability; провал распространяется с путём `<collection>[].<entry_property>`, не глотается. Новые коды: `core:diagnostic.ui_observability.no_entry_widget_class` (нет класса для инстанцирования), `core:diagnostic.ui_observability.entry_not_property_host` (класс есть, интерфейса нет).
    - Рекурсия сразу нашла 2 реальных дефекта в продакшне: `UGV2ModalWidgetBase::DescribeUiCapabilities` хардкодил bare-класс `UGV2ButtonWidgetBase` для своей коллекции "buttons" вместо делегирования в уже существующий `ButtonList->ResolveButtonWidgetClass()` (bare-класс без `WidgetTree` — `text`-capability кнопки была ни к чему не привязана); `WBP_PlayerStatusPanel.IconWidgetClass` указывал на `WBP_Image` (`ScalePolicy=Unset`) вместо `WBP_Icon` (`PreserveAspect`) — найдено через unreal-mcp (`ObjectTools.get_properties`/`set_properties`, `AssetTools.save_assets`), исправлено на живом активе. Оба зафиксированы и красным тестом на откате (`git stash` на `GV2ModalWidgetBase.cpp`/`GV2UiCapabilityObservability.cpp` — `GV2.UI.CapabilityObservabilityCompositeSweep` красный на обоих, `git stash pop` — снова зелёный).
    - Три явных сценария подделки (`GV2ForgeryTestWidgets.h/.cpp` — новый test-only `UGV2ForgeryEntryTestWidget`, `Tests/GV2UiCapabilityObservabilityTests.cpp` — `GV2.UI.CapabilityObservabilityCollectionForgery`), каждый — capability внутри синтетической коллекции, не на самом хосте: (1) *consumer заменён на no-op* — `SetBindingHandle` ничего не сохраняет, `GetBindingHandle` всегда возвращает фиксированное значение → `not_distinguishable`; (2) *renderer отвязан* — `TargetName` указывает на несуществующего потомка (bare-класс без `WidgetTree`) → `prepare_or_commit_failed`; (3) *capability объявлена без реализации* — `StableId` с `TargetKind`, отличным от `"resource"` → `MakeDistinctValuePair` не может синтезировать пару → `no_distinct_pair`. Красный тест на откате: `git stash` только `GV2UiCapabilityObservability.cpp/.h` (рекурсия убрана) — все три сценария красные (`TestFalse` на "harness must reject" не проходит: без рекурсии `CollectionHost` молча пропускается); `git stash pop` — снова 97/97 зелёных.
      - Примечание: для CDO fresh-инстанса, который рекурсия создаёт через `CreateWidget`, обычный `UPROPERTY`-член, установленный на CDO в рантайме, не копируется в новый инстанс (эмпирически проверено) — режим подделки передаётся через `static EGV2ForgeryMode& ModeForNextInstance()`, а не через поле CDO.
    - Охват sweep (`GV2.UI.CapabilityObservabilityCompositeSweep`) переведён с 7 захардкоженных путей на `FAssetRegistryModule::GetAssets` по `/Game/UI`, `/Game/TextSystem/UI`, `/Game/RH/UI` + фильтр `WidgetClass->ImplementsInterface(UGV2UiPropertyHost::StaticClass())` — будущий `WBP_*`, реализующий интерфейс, попадёт в sweep без правки теста. `WBP_Modal` (не основан на `UGV2ModalWidgetBase`) остаётся документированным исключением — синтезируется вручную, как и раньше.
    - Расширение охвата на все хосты (не только 7 композитов-экранов) вскрыло ещё 3 реальные находки, тоже исправленные в этом change set:
      - `UGV2ButtonListWidgetBase` объявлял `key`-capability на себя (`AddKey("key", NAME_None)`), но не реализовывал ни `SetKey`/`GetKey`, ни ветку в `FGV2KeyPropertyConsumer::Commit`/`Reset` — capability, буквально не имевшая как быть исполненной. Добавлены `SetKey`/`GetKey`/`Key`, ветки в consumer'е, чтение в `CaptureUiTargetState`.
      - `UGV2ListViewWidgetBase::DescribeUiCapabilities` объявлял коллекцию "items" без `EntryWidgetClass` — но эта коллекция никогда не проходит через generic Prepare/Commit pipeline: реальное использование (см. `ResolveXxxRepeater()` в `GV2LocationCompositeWidgetBases.cpp`) — это C++-шаблоны `ReconcileEntries`/`ReconcilePreparedEntries`, вызываемые композитом напрямую, entry-класс не фиксирован на уровне класса. Объявление удалено (`DescribeUiCapabilities` теперь пуст) — declaring what can't be honestly verified is the same lie the plan targets.
      - `WBP_Image.ScalePolicy` был `Unset` — собственная `resource_id`-capability не проходила harness ни при каком пробном ресурсе. Актив не используется ни одним другим виджетом (проверено `grep` по `Content/`), но лежит в зоне будущего риска (как ранее `WBP_PlayerStatusPanel`, указавший на него по ошибке) — исправлено на `FreeStretch` через unreal-mcp.
      - Тест-гейт `SweepClass`: строгая проверка "хотя бы одна capability верхнего уровня" заменена на honest-info для нулевого дерева (генерик-примитив вроде `ListView` легитимно может не объявлять ничего свои), при этом требование "всё объявленное — наблюдаемо" осталось безусловным.
    - Верификация: 97/97 UE Automation (headless, `-nullrhi`), 68/68 Headless ctest, все 6 content/doc-гейтов зелёные.

- [ ] **PCC-11 — Контракты описывают фактический API**
  - `WidgetRegistry.md` перечисляет native-классы как реализующие `IGV2ScreenFieldHost`, хотя в коде его реализуют только четыре композита локации. Расхождение опасно тем, что читатель контракта планирует работу по несуществующей поверхности.
  - Done: [Widget Registry](../../UI/WidgetRegistry.md), [Screen Templates](../../UI/ScreenTemplates.md) и [UI Document](../../UI/UIDocumentAndReconciliation.md) описывают фактические интерфейсы, фактический набор их реализаций и фактическую семантику reset после PCC-07; ни одно нормативное утверждение об атомарности не сильнее того, что подтверждает тест — проверено выборочно по каждому такому утверждению; расхождения, не устранённые в этом плане, записаны строкой `STATUS-NNN`, а не оставлены в тексте контракта.
  - Evidence: `Docs/UI/`, `Docs/Status/ImplementationStatus.md`.

- [ ] **PCC-12 — Сверка закрытий**
  - Зависимости: PCC-10, PCC-11.
  - Done: для каждой находки `UPP-R1`, `UPP-R2`, `UPP-R3`, `UPP-R6`, `UPP-R7` названа проверка и **продемонстрировано**, что она краснеет при откате соответствующего изменения; отдельно подтверждено, что закрыт класс, а не экземпляр: попытка воспроизвести дефект того же вида в другой точке pipeline отклоняется гейтом; расхождения, найденные сверкой, либо устраняются в этом же change set, либо записываются строкой `STATUS-NNN` — «закрыто, но не проверено» исходом не является; ревью переносится в `Docs/Status/Archive/` по процедуре `AGENTS.md`.
  - Evidence: отчёт change set, `Source/GV2/Private/Tests/`, `Docs/Status/Archive/`.

## Проверка milestone

- [x] Sweep покрывает элементы коллекций и краснеет на подделке объявления внутри них (PCC-10).
- [ ] Ни один контракт не утверждает больше, чем подтверждает тест.
- [ ] Каждая находка ревью закрыта продемонстрированным красным тестом.
