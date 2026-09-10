---
title: Apply Boundary Tasks
status: active
version: 1.7
updated: 2026-09-09
depends_on:
  - README.md
  - Payload.md
  - ../../ADR/0043-presentation-apply-boundary.md
---

# M4 — Apply Boundary

> **Материализует:** `D2/D4` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md).
> **Задачи:** PSC-11…12.
> **Результат:** физическое применение находится в нижнем модуле с одной entry point; последующая `UCLASS`-миграция не оставляет сломанного commit.

## Физическая граница

`GV2PresentationApply` содержит resolved DTO, prepared operations, **физическую часть** Commit/Reset/Rollback — то есть каждую запись в виджет, — keyed reconciliation, восстановление проекции, physical widget bases и pure layout/viewport calculations. Это формулировка `D2` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md) («физическое применение — Commit, откат, keyed-реконсиляция, восстановление проекции…»), а не её ослабление: квалификатор «физическое» относится ко всему перечню.

Наверху остаётся **решение**, какие записи выполнить и какие откатить. Это не выбор удобства: rollback plan строится из `FGV2PreparedUiObject` и compiled schema (`GV2ContentCore::FCompiledUiFieldSpec`), а `GV2ContentCore` нижнему модулю запрещён графом сборки — перенести решение вниз означало бы затащить туда авторитетные типы, ради недостижимости которых граница и существует. Проверяемое следствие: ни один consumer и ни один reconciler не выполняет запись в виджет сам, всё уходит в единственный façade; для фактического множества consumer kinds это проверяет `validate_property_consumer_transaction_coverage`.

Разрешённые module dependencies:

```text
Core, CoreUObject, Engine, UMG, CommonUI, Slate, SlateCore
```

Запрещённые dependencies/capabilities:

```text
GV2, GV2ContentHostSupport, DeveloperSettings, AssetRegistry, ImageCore,
content authoring modules, filesystem/config discovery, soft/synchronous loading
```

Build graph доказывает отсутствие project authority types. Поскольку `CoreUObject/Engine` сами предоставляют soft-loading API, отдельный source-tree gate запрещает эти capabilities внутри модуля и честно остаётся вторичным к module boundary.

## Задачи

- [x] **PSC-11 — Завершить `GV2PresentationApply` и запретить обратную зависимость**
  - Зависимости: PSC-10A, PSC-10B, PSC-10C.
  - Инвариант: Apply получает только `FGV2PreparedPresentationTransaction`; новый authority с любым именем недоступен lower module по dependency direction.
  - Не считается закрытием: соглашение без `Build.cs`; несколько public apply paths; вызов upper callback; сохранение Commit/rollback логики в thin adapters; утверждение, что module graph сам запрещает `LoadSynchronous()`.
  - Done:
    - `Source/GV2PresentationApply/GV2PresentationApply.Build.cs` использует точный allowlist выше и не имеет conditional authority/editor dependencies;
    - одна public façade `FGV2PresentationApply::Apply(FGV2PreparedPresentationTransaction&, FGV2PresentationApplyResult&)` является единственной production entry point применения целой transaction;
    - физическая часть Commit, Reset и rollback — каждая запись в виджет, — а также keyed reconciliation, восстановление проекции и pure viewport/layout calculations реализованы ниже façade; решение, какие записи выполнить и какие откатить, остаётся выше по причине, названной в «Физической границе»;
    - верхний `GV2` выполняет semantic Prepare и одним вызовом передаёт готовую transaction; временные методы существующих `UCLASS` только делегируют и не содержат mutation/lookup logic;
    - transitive include closure будущих moved widget bases классифицировано механически: authority-free value/physical interfaces принадлежат lower module, authority-aware types остаются в `GV2` за DTO boundary, duplicate bridge types отсутствуют;
    - exported-public inventory модуля выводится из его `Public/` declarations и классифицирует façade, DTO/results и локальные widget/lifecycle methods; вторая transaction apply entry point запрещена;
    - actual UBT graph выводится из всех `*.Build.cs`; forbidden edge и conditional edge отвергаются, negative self-test добавляет synthetic edge;
    - actual CMake target/source graph доказывает, что portable/Headless targets не линкуют и не компилируют `GV2PresentationApply`;
    - весь source tree модуля перечисляется обходом каталога; secondary forbidden-capability gate отвергает `TSoftObjectPtr` runtime input, `LoadSynchronous`, `StaticLoadObject`, Asset Registry, filesystem/config/settings access;
    - каждый forbidden-capability case имеет synthetic negative self-test;
    - runtime authority counter показывает accesses во время Prepare и ноль вокруг единственной Apply façade для каждого operation kind, включая central style;
    - production initial screen, replacement, nested collection, rollback и catastrophic recovery проходят через façade;
    - task не меняет ни одного Widget `UCLASS` module/path;
    - **предпосылка проверена до переноса**: ни один класс, подлежащий переносу, не достигает авторитета — ни через виды операций, ни через central style, ни через image resource resolution в widget lifecycle (`PSC-10C`). Это результат `PSC-10A`/`PSC-10B`; здесь он не переделывается, а подтверждается implementation/call-site inventory. Runtime `NativePreConstruct` не является обходом façade; design-time branch применяет только сериализованные value defaults и не получает authority capability.
  - Evidence: `Source/GV2PresentationApply/`, `Source/GV2/GV2.Build.cs`, `Source/CMakeLists.txt`, `Headless/CMakeLists.txt`, graph/API/capability gates и production tests.

  - **Реализация (2026-09-09).**

    **Второй вход убран, а не переименован.** До задачи диспетчеризация жила в двух местах: собственный visitor нижнего модуля для plain Engine/UMG целей и `GV2LegacyPresentationApplyAdapter` для GV2-owned `UCLASS`'ов, которые нижний модуль не может привести по типу. Каждый вызывающий обязан был помнить оба, и «транзакция применена» не было одним фактом. Adapter удалён; `FGV2PresentationApply::Apply(Transaction, Result)` — единственный вход.

    Как это сделано **без переноса `UCLASS`** (перенос — `PSC-12`): GV2-owned цели достаются через **value-only ролевые интерфейсы нижнего модуля**. Виджет объявляет, какое физическое действие он умеет; нижний модуль достаёт его через роль, а не через конкретный тип, который ему нельзя назвать. Каждая роль — **отдельный** интерфейс намеренно: один интерфейс с no-op методами позволил бы виджету объявить участие, забыть override и отчитаться об успешном commit'е, ничего не записав, — ровно та форма, ради устранения которой существует конвейер. Отсутствие роли у цели диагностируется.

    `IGV2UiPropertyHost` и `IGV2UiBindingTarget` **выведены** из соответствующих ролей, а не продублированы на каждом классе: это сохраняет `DUC-03` — новому хосту, объявляющему capability `key`, по-прежнему не нужны правки нигде.

    **Найдено при переносе:** `ApplyText` у `Text`/`RichText` читал bare `BindWidget`-член, тогда как снятый adapter перенаправлял commit через `GetTextBlock()`/`GetRichTextBlock()` с их name-lookup fallback. На инстансе, чей член не связан, новый путь отвергал текст, который старый применял (поймано `CapabilityObservabilityHarness`). Обе точки переведены на аксессоры.

    **Ниже façade перенесено:** `FGV2KeyedCollection` (сопоставление детей по ключам и перестроение children панели — физическое применение, никогда не зависевшее от типов GV2) и pure viewport/layout вычисления (уже там с `PSC-10A`/`PSC-10C`). Commit/Reset/rollback остаются семантической оркестровкой в `GV2`: они решают, *какие* записи и как их откатить, а сами записи целиком выполняет façade — что и проверяется гейтом покрытия транзакции для фактического множества consumer kinds.

    **Классификация видов операций переосмыслена.** Вместо «пишет нижний модуль» / «оставлено адаптеру» — три исхода: запись plain-цели, требование объявленной роли, и отказ при её отсутствии. Третий — то, ради чего роли разделены; он же не даёт вернуть «молчаливый успех» под видом no-op.

    **Гейты.** `validate_presentation_apply_surface` выводит экспортируемую поверхность из `Public/`-объявлений модуля и классифицирует её по ролям; правило про второй вход **читает сигнатуру, а не имя** — переименованный второй вход остаётся вторым. Forbidden-capability скан перечисляет всё дерево модуля обходом каталога (soft-ссылки, синхронная загрузка, Asset Registry, settings, файловая система, config) и явно назван вторичным к module graph: `CoreUObject`/`Engine` сами дают эти API, и никакой скан не является полной классификацией будущих UE API. `validate_presentation_apply_module_graph` расширен до фактического графа из **всех** `*.Build.cs`, отвергает условное ребро (зависимость за условием — утверждение про одну конфигурацию, а не про граф) и второго потребителя модуля, и отдельно проверяет, что portable/Headless CMake не компилирует и не линкует его. `validate_central_style_runtime_boundary` теперь выводит Apply-ветки из façade нижнего модуля и сверяет объявленные ролевые интерфейсы с реализованными. Каждое правило имеет synthetic self-test в обе стороны.

    **Include closure для `PSC-12` классифицирован заранее** (`validate_apply_move_closure`): множество переносимых классов выводится из того, что класс выполняет хотя бы одну prepared-роль, а не из списка; его GV2-включения делятся на value/physical (могут уехать вместе с ним) и authority-aware (остаются за DTO boundary). Authority-aware множество зафиксировано как baseline и объявлено храповиком: сжиматься можно, расти нельзя. Гейт честно назван храповиком, а не доказательством. Классификация «value/physical» дополнительно проверяется: такой заголовок не должен называть `FGV2PresentationPrepareContext`, `FGV2SessionContentSnapshot`, каталог, реестр или `UGV2UiTheme`, иначе это ярлык, а не факт.

    **Направление авторитета измерено на production-пути.** `FGV2PresentationPrepareContext` считает обращения — все они идут через его аксессоры. `GV2.Runtime.Presentation.ApplyReadsNoAuthority`: Prepare читает снимок (счётчик > 0, то есть утверждение не вакуумно), окно вокруг единственного façade читает его ноль раз, и число применённых операций равно числу подготовленных. В самом тесте записано, что это **вторичное** свидетельство: первично то, что граф модуля делает тип авторитета неназываемым внутри Apply, и никакой счётчик не доказывает отсутствие возможности, которую модуль не может слинковать.

    Ни один Widget `UCLASS` не сменил модуль или путь.

    Верификация: `Automation RunTests GV2` — 134/134; `ctest` — 100/100; 16 гейтов и их self-test'ы; `validate_docs` — 185 файлов; `CompileAllBlueprints` — 0 errors, 0 warnings.

  - **Сверка после закрытия (2026-09-09).** Отдельный проход по `PSC-01…11` против исходной цели плана нашёл три расхождения; все исправлены здесь, а не перенесены.

    1. **Регрессия `DUC-03`, внесённая этой задачей.** Снятый adapter маршрутизировал `selected_key`/`default_tab_key` по имени и отвечал `core:diagnostic.ui_consumer.unhandled_target`, когда цель не была классом-владельцем. Новый общий `IGV2UiPropertyHost::ApplyPreparedKey` принимал **любое** имя и писал значение в собственный `key` хоста — успешный commit, записавший не в то поле, то есть ровно та форма отказа, ради устранения которой существует конвейер. Существующий тест закрывал только «цель вообще не property host», поэтому регрессия прошла зелёной. Правило восстановлено (`IsHostClaimedKeyCapability`), а множество имён теперь сверяется с фактически объявленными capability рефлексией по всем native `IGV2UiPropertyHost` (`GV2.UI.PreparedKeyCapabilityRouting`) — объявить новое именованное key-capability, не маршрутизировав его, теперь нельзя молча.
    2. **Мёртвая цель central style стала отказом транзакции.** Adapter возвращался из операции, если `TargetWidget` уже собран GC; façade вместо этого сообщал `central_style_target_mismatch … '<null>'` и валил всю транзакцию. Ранний возврат восстановлен: мёртвый weak pointer — это «писать больше некуда», а не несоответствие роли, и так же трактуется каждой другой операцией.
    3. **Восстановление проекции оставалось выше границы.** `FGV2LayeredUiReconciler::CommitReconcile` откатывал порядок детей слоя собственным `ClearChildren`/`AddChild`, а модальную интерактивность писал прямо в виджет. И то и другое — физическое применение, названное в `D2`. Первое сведено к `FGV2KeyedCollection::RestoreOrder` в нижнем модуле (тот же перестроительный цикл, который примитив уже выполнял внутри себя), второе — к `UGV2GameShellWidgetBase::SetTopModalInteractive`, то есть к методу виджет-базы, который уезжает вниз в `PSC-12`. Реконсилятор больше не пишет в виджеты сам.

    Там же исправлены утверждения, ставшие ложными: ~20 комментариев описывали удалённый adapter как действующий механизм; гейт `validate_central_style_runtime_boundary` держал исключение, ключом которого было имя удалённого файла — то есть готовую лазейку; `Docs/UI/README.md` называл поверхность холодного восстановления одним классом, тогда как фактических файлов в этой роли два.

- [ ] **PSC-12 — Атомарно мигрировать Widget `UCLASS` paths и ассеты**
  - Зависимости: PSC-11.
  - Инвариант: до task дерево целиком использует `/Script/GV2`; после task — `/Script/GV2PresentationApply`; ни один commit не содержит смешанную или неразрешимую модель.
  - Не считается закрытием: перенос classes в `PSC-11`; постоянные redirects; известный список ассетов; пересохранение только `/Game/UI`; source-only проверка без загрузки Blueprint; попытка довести прерванную миграцию вручную вместо возврата к точке отката.
  - Done:
    - перед изменением paths зафиксированы baseline Asset Registry inventory и успешная загрузка/компиляция всех Widget Blueprint, наследующих или ссылающихся на переносимые classes;
    - множество переносимых `UCLASS` выводится из фактической inheritance/dependency closure физических widget bases, а не из списка задачи;
    - в одном рабочем change set добавляются временные Core Redirects, classes переносятся, каждый affected asset загружается, компилируется и сохраняется через Unreal Editor API;
    - после resave Asset Registry/full-package sweep не находит old `/Script/GV2` class references;
    - redirects удаляются до commit, Editor перезапускается/перезагружает packages без них, повторный полный load/compile sweep проходит;
    - widget blueprint count и component contract сравниваются с baseline, который не меняется в том же task;
    - `unreal-mcp` сообщает успешные load/compile/save для фактического affected set; failed/unavailable MCP блокирует `[x]`;
    - commit содержит C++ path move и все affected UAssets вместе; промежуточное состояние не фиксируется;
    - **точка возврата названа явно и проверена до начала**: ею является `HEAD` на момент начала миграции — тот коммит, с которого стартует единственный change set задачи; его hash фиксируется в записи о реализации, а не заранее в этом пункте, потому что записанный заранее hash перестаёт указывать на нужное дерево при любом коммите между записью и стартом. Проверяемое свойство точки возврата одно: дерево на ней целиком использует прежние пути `/Script/GV2`; при обрыве миграции на любом шаге — включая частично пересохранённые ассеты и неснятые редиректы — восстановление выполняется возвратом рабочего дерева к этому коммиту целиком, а не доведением наполовину мигрированного состояния.
  - Evidence: `Source/GV2PresentationApply/`, удалённые/перенесённые `Source/GV2/Public|Private/UI` classes, `Content/`, временный diff `Config/DefaultEngine.ini`, Asset Registry reports и Unreal MCP results.

## Проверка milestone

- [ ] Project authority type не может попасть в Apply module через UBT edge.
- [ ] UE loading/settings/filesystem capability отвергается отдельным full-source-tree gate.
- [ ] Headless/CMake graph не содержит Apply module или его sources.
- [ ] До `PSC-12` class paths не меняются; после него старые paths и redirects отсутствуют.
- [ ] Все найденные Widget Blueprint загружаются и компилируются после чистого reload.
