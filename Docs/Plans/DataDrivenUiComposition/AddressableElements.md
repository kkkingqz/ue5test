---
title: Addressable Elements Tasks
status: active
version: 1.4
updated: 2026-09-01
depends_on:
  - README.md
  - ../../UI/WidgetRegistry.md
---

# M1 — Addressable Elements

> **Материализует:** барьер «базовые элементы не адресуемы» и генерализацию `key`.
> **Задачи:** DUC-01…04.
> **Результат:** любой хост свойств может быть полем объемлющего хоста, и добавление нового хоста не требует правки общего файла.

## Результат этапа

Механизм адресации уже спроектирован правильно: `ScreenFieldId` — это `UPROPERTY(EditAnywhere)`, задаваемое на каждом размещении в Designer, а экран находит поля обходом `WidgetTree`. Проблема исключительно в охвате: свойство есть у четырёх классов из восемнадцати.

Параллельно снимается вторая привязка к конкретным классам — `key`. Тринадцать из пятнадцати веток в `FGV2KeyPropertyConsumer::Commit` буквально идентичны и отличаются только типом, к которому приводится указатель.

## Задачи

- [x] **DUC-01 — Одно свойство идентичности внутри объемлющего хоста**
  - Экран и блок различаются только охватом верификации, поэтому вводить два свойства нельзя: автор ассета получит два способа выразить одно и то же и будет угадывать, какой действует.
  - Done: свойство идентичности объявлено один раз в общей поверхности хостов свойств, а не по классам; его смысл зафиксирован в контракте как «идентичность внутри объемлющего хоста» с явным указанием, что на уровне экрана это field_id, а на уровне композита — имя свойства; свойство доступно в Designer на каждом размещении; существующие четыре композита локации переведены на него без изменения поведения; дубликат идентичности внутри одного хоста отклоняется до `Ready`.
  - Evidence: `Source/GV2/Public/UI/`, `Docs/UI/ScreenTemplates.md`.
  - **Реализация (2026-09-01):**
    - `FGV2UiPropertyHostState` (`GV2UiPropertyHost.h`) переведён в `USTRUCT()` и получил `UPROPERTY(EditAnywhere, meta=(DisplayName="Host Identity")) FName HostIdentity;` + `GetHostIdentity()`/`SetHostIdentity()`. `IGV2UiPropertyHost` получил такие же неprямые (non-virtual) методы-обёртки, читающие/пишущие через `GetPropertyHostState()` — единая точка объявления для абсолютно любого будущего `IGV2UiPropertyHost`, а не per-class свойство.
    - Все 4 Location-композита (`GV2LocationCompositeWidgetBases.h`): удалены собственные `UPROPERTY(EditAnywhere) FName ScreenFieldId;` (и парная документация "UPP-27"); `IGV2ScreenFieldHost::GetScreenFieldId()` теперь делегирует в `GetHostIdentity()`; их `PropertyHostState`-член сделан `UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))`, чтобы `HostIdentity` был виден и редактируем в Designer на каждом размещении без лишнего уровня группировки.
    - **Живой дефект, найденный при первом прогоне**: перенос свойства осиротил значения, уже сохранённые в `WBP_LocationScreen.uasset` под именем `ScreenFieldId` — при загрузке UE молча теряет значение переименованного/удалённого `UPROPERTY` без redirect. Оба живых теста (`GV2.Runtime.Presentation.RhStartOpensLocationScreen`, `GV2.Runtime.UI.LocationScreenTransitionContract`) покраснели с `payload contains unknown field 'commands'` — сконфигурированный host для поля "commands" исчез. Исправлено через `unreal-mcp` (`UMGToolSet.GetWidgets` → 4 refPath дочерних композитов → `ObjectTools.set_properties({"propertyHostState":{"hostIdentity":"..."}})` для `top_bar`/`player_status`/`scene`/`commands`, `AssetTools.save_assets`); проверено `AutomationTestToolset.RunTestsByFilter` внутри живого редактора — оба теста зелёные, затем подтверждено headless (98/98).
    - Новый тест `GV2.Runtime.Presentation.HostIdentityIsSharedNotPerClass`: (1) `UGV2TextWidgetBase` — класс, никогда не реализовывавший `IGV2ScreenFieldHost` и не имевший собственного понятия идентичности — получает и возвращает `HostIdentity` тем же способом, что и Location-композит, что доказывает объявление на общей поверхности, а не по классам; (2) два `UGV2LocationTopBarWidgetBase` с одинаковым `HostIdentity` в одном экране — `GetScreenFieldIds()` возвращает пустой массив (отклонено, не тихо принято/задвоено); с различными идентичностями — оба присутствуют.
    - Красный тест на откате подтверждён: временная замена `GetScreenFieldId()` у TopBar на всегда `NAME_None` (симулирует «идентичность не подключена к общей поверхности») — новый тест красный ровно на assertion "distinct identities on the same host are accepted"; восстановление — снова зелёный (98/98).
    - Документация: `Docs/UI/ScreenTemplates.md` получил раздел «Host Identity (DUC-01)», фиксирующий единственное значение свойства на обоих уровнях (`field_id` / имя свойства композита) и Designer-поверхность.
    - Верификация: 98/98 UE Automation, 68/68 Headless ctest, все 6 content/doc-гейтов зелёные.

- [x] **DUC-02 — Базовые элементы становятся адресуемыми**
  - Зависимости: DUC-01.
  - `UGV2TextWidgetBase`, `UGV2ImageWidgetBase`, `UGV2ProgressBarWidgetBase`, `UGV2ButtonWidgetBase`, Checkbox, InputField, RichText, Portrait уже реализуют `IGV2UiPropertyHost` и имеют одинаковую сигнатуру наследования; не хватает только адресуемости.
  - Done: каждый базовый элемент может быть полем объемлющего хоста; экран, собранный из базовых элементов с проставленными в Designer идентичностями, применяет поля без единого нового C++-класса — проверено тестом, который загружает такой ассет и прогоняет значения со стороны Lua; элемент без заданной идентичности считается неконфигурированным и пропускается, как и раньше; неканоническая идентичность отклоняется с диагностикой.
  - Evidence: `Source/GV2/Public/UI/`, `Content/`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-09-01):**
    - Все 8 названных классов получили `, public IGV2ScreenFieldHost` в списке наследования и одну и ту же делегирующую реализацию `virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }` (тот же однострочник, что у Location-композитов) + `PropertyHostState` переведён в `UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))`, чтобы `HostIdentity` стал редактируемым в Designer на каждом размещении. Composite/collection-виджеты вне списка (`DropdownSelect`, `ButtonList`, `Modal`, `TabContainer`) и transient-проекция (`RichTextPopover`) сознательно не тронуты — они вне границ этой задачи.
    - Живая демонстрация «экран из базовых элементов без нового C++-класса»: в `WBP_Testscreen` через `unreal-mcp` (`UMGToolSet.AddWidget`) добавлен шестой child — `GreetingText` (`WBP_Text`, класс `UGV2TextWidgetBase` — уже существующий базовый элемент, не новый), `PropertyHostState.HostIdentity = "greeting"`; `GameData/sample/scripts/debug/start.lua` публикует поле `greeting` (`core:schema.ui_field.text.v1`) вместо пустого `fields = {}`.
    - **Найденный и устранённый живой дефект (не в C++, в content pipeline)**: значение `text_id` в `FGV2UiPropertyHostState`/Screen Field pipeline резолвится через `Theme->TextCatalog` (`TMap<FName,FText>` на `DA_UITheme_Default`) — это **не** автоматически синхронизированная с `GameData/*/definitions/texts.json5` структура, а вручную поддерживаемый ассет. Добавление `sample:text.screen.test.greeting` только в `texts.json5` дало `ResolveText: ... failed: Unknown text_id` при первом реальном прогоне через Lua (диагностика добавлена этой задачей: `GV2ScreenFieldMaterializer.cpp` теперь логирует причину при несостоявшемся `GetCompiledSchema`/`ProjectMaterializedValue`/`ResolveText`, которые раньше падали молча). Устранено: запись добавлена в `DA_UITheme_Default.TextCatalog` через `unreal-mcp` (`ObjectTools.set_properties`).
    - **Побочная находка, НЕ устранённая (вне границ задачи)**: все прочие `sample:text.screen.test.*` id из `texts.json5` (`checkbox`, `name_label`, `dropdown_placeholder` и т.д.) в `DA_UITheme_Default.TextCatalog` хранятся под старым namespace `core:text.screen.test.*` — расхождение, пережившее переименование пакета `core`→`sample`, которое никогда не проявлялось, потому что ни один из них не проходил через реальный `ResolveText` (только через прямые `FGV2TextViewModel`-конструкции в unit-тестах). Не исправлено в этой задаче — вынесено отдельной задачей на дозволенный follow-up.
    - Расширен `GV2.Runtime.Presentation.LuaCreatesRegisteredScreen`: `GetScreenFieldIds()` теперь ожидаемо `["greeting"]` (не `0`), `GreetingText`-виджет получает точное текстовое значение через полный реальный путь Lua → `GV2ScreenFieldMaterializer` → `PrepareScreenFields`/`CommitScreenFields`. Пять исходных static leaves остаются непроверенными на identity (как раньше) — Done-критерий «элемент без идентичности пропускается» подтверждён тем же тестом.
    - Новый тест `GV2.Runtime.Presentation.BaseElementNonCanonicalIdentityRejected`: синтетический `UGV2TextWidgetBase` с `HostIdentity = "Not-Canonical"` — `GetScreenFieldIds()` отклоняет с диагностикой `has non-canonical field_id`, используя тот же `IsCanonicalFieldId`, что и Location-композиты (не новый, отдельный путь проверки).
    - Красный тест на откате подтверждён: временный `GetScreenFieldId()` → `NAME_None` на `UGV2TextWidgetBase` — `LuaCreatesRegisteredScreen` красный целиком (`payload contains unknown field 'greeting'`, вся сессия не стартует, поскольку сконфигурированный host без ответа — инвариант, а не мягкий пропуск); восстановление — 99/99 зелёных.
    - `WidgetRegistry.md`/`ScreenTemplates.md` обновлены: таблица implementations, footnote, секция "Host Identity", секция "Current WBP_Testscreen contract" и один найденный по пути устаревший verification-пункт (`WBP_Testscreen содержит deterministic ... required` — противоречил уже исправленной в PCC-11 секции того же файла).
    - Верификация: 99/99 UE Automation, 68/68 Headless ctest, все content/doc-гейты зелёные.

- [x] **DUC-03 — `key` перестаёт быть перечислением классов**
  - `FGV2KeyPropertyConsumer::Commit` перечисляет 15 классов; 13 веток идентичны. Новый хост, объявивший `key`, обязан быть добавлен туда вручную — и до недавнего исправления его отсутствие давало тихий успех, что и произвело дефект `UGV2ModalWidgetBase`.
  - Done: ключ хранится в общем состоянии хоста свойств, consumer сводится к одной ветке; две семантически иные capability — выбранный ключ выпадающего списка и ключ вкладки по умолчанию — разведены по именам и не выдают себя за `key`; новый хост, объявивший `key`, работает **без** правки consumer — проверено тестом на классе, добавленном только в тесте; типизированный отказ на неподдерживаемый тип цели сохраняется как защита.
  - Evidence: `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, `Source/GV2/Public/UI/GV2UiPropertyHost.h`.
  - **Реализация (2026-09-01):**
    - `FGV2UiPropertyHostState` (`GV2UiPropertyHost.h`) получил `UPROPERTY(Transient) FName Key;` + `GetKey()`/`SetKey()` — тем же способом, что `HostIdentity` в DUC-01: единственное объявление на общей поверхности хостов, а не по классам. `IGV2UiPropertyHost` получил неprямые `GetKey()`/`SetKey()`, делегирующие в `GetPropertyHostState()`.
    - Все 15 классов, ранее хранивших собственный приватный `FName Key;` (13 базовых/композитных виджетов плюс `UGV2ButtonWidgetBase` отдельно), переведены на общее хранилище: приватное поле удалено, `SetKey`/`GetKey` делегируют в `GetPropertyHostState()`. В `GV2ButtonWidgetBase.cpp` заодно исправлена оставшаяся ссылка на удалённое поле (`OnActivated.Broadcast(Key)` → `OnActivated.Broadcast(GetKey())`).
    - `FGV2KeyPropertyConsumer` (`GV2PropertyConsumers.h/.cpp`) получил `SetPropertyNameForRouting()` и приватное поле `PropertyName`, заполняемое в `Prepare()` из `Capability.PropertyName`. `Commit`/`Reset` переписаны с 15 веток по типу цели на маршрутизацию по имени свойства: `selected_key` → `DropdownSelect`, `default_tab_key` → `TabContainer`, любое другое имя (включая `key`) → общий `IGV2UiPropertyHost::SetKey`/`GetKey`. Для сброса, у которого consumer создаётся через `FGV2PropertyConsumerFactory::CreateConsumer` без прохода через `Prepare()`, `PropertyName` теперь проставляется явно в обеих точках создания в `GV2UiMutationPlan.cpp` через `SetPropertyNameForRouting()`.
    - **Найденный и устранённый живой дефект**: `UGV2TabContainerWidgetBase` объявляет одновременно `default_tab_key` и собственный `key` как отдельные capability на себя же, но старый consumer маршрутизировал ЛЮБОЙ key-kind коммит на неё в `ApplyDefaultTabKey`, различая цели только по типу виджета, а не по имени свойства — собственная идентичность (`key`) TabContainer тихо подменялась значением вкладки по умолчанию. Новая маршрутизация по имени свойства устраняет это как естественное следствие редизайна; обнаружено тестом `GV2.UI.CapabilityObservabilityCompositeSweep`, упавшим с `capability 'key' is not observable` — до этой задачи `CaptureUiTargetState` вообще не читал `key` у TabContainer.
    - `CaptureUiTargetState` (`GV2UiCapabilityObservability.cpp`) консолидирован: 11 однострочных per-class блоков `key="..."` (Button, Checkbox, InputField, ProgressBar, TopBar, PlayerStatus, Scene, CommandPanel, Modal, ButtonList, RichTextPopover) заменены одним общим блоком через `Cast<IGV2UiPropertyHost>(TargetWidget)->GetKey()`; у Portrait/RichText/Image из блоков убрана только строка `key=`, остальное содержимое (`portrait_brush=`/`rich_text=`/`image_brush=`) сохранено. Это одновременно закрыло находку выше — TabContainer's `key` стал наблюдаем впервые.
    - Новый тестовый класс `UGV2NewHostAddedOnlyInTestWidget` (`GV2ForgeryTestWidgets.h/.cpp`) — реализует голый `IGV2UiPropertyHost` и объявляет `key`-capability, не упоминаясь нигде в `GV2PropertyConsumers.cpp`. Тест в `GV2PropertyConsumersTests.cpp` прогоняет `Prepare`/`Commit`/`Reset` прямо на нём и отдельно проверяет типизированный отказ `unhandled_target` на цели без `IGV2UiPropertyHost` (`NewObject<UVerticalBox>`).
    - Красный тест на откате: удаление общей ветки `IGV2UiPropertyHost` из `Commit()` дало не «чистый» упавший assert, а `SIGSEGV` внутри `GV2PropertyConsumersTests.cpp:653` — коллекционный consumer (`FGV2KeyedCollectionPropertyConsumer`) внутренне реконсилирует `key` каждого элемента через тот же путь, и без общей ветки элемент не регистрируется в ListView по ключу, а тест разыменовывает `nullptr` без предварительной проверки. Каскадный крах через несвязанные секции теста расценён как более сильное свидетельство, чем изолированный failure; ветка восстановлена, подтверждено git diff.
    - `Docs/UI/WidgetRegistry.md`: описание `FGV2KeyPropertyConsumer` в списке стандартных потребителей обновлено — вместо `(key, selected_key)` теперь описывает маршрутизацию по имени capability, включая `default_tab_key`.
    - Верификация: 99/99 UE Automation, 68/68 Headless ctest, все 6 content/doc-гейтов зелёные.

- [x] **DUC-04 — Охват sweep следует за адресуемостью**
  - Зависимости: DUC-02, DUC-03.
  - Done: список классов, прогоняемых sweep наблюдаемости, выводится из фактического набора реализаций `IGV2UiPropertyHost`, а не перечисляется в тесте вручную; добавление хоста без покрытия sweep краснит сборку; проверка идентичности включена в sweep как обычная capability.
  - Evidence: `Source/GV2/Private/Tests/GV2UiCapabilityObservabilityTests.cpp`.
  - **Реализация (2026-09-01):**
    - `GV2.UI.CapabilityObservabilityCompositeSweep` получает direct native implementation boundaries `IGV2UiPropertyHost` reflection-ом из `/Script/GV2`, а затем требует реальный `WBP_*`-потомок для каждого из них. Новый production boundary без asset fixture краснит automation; два test-only forged host исключены только явно через `UCLASS(meta=(GV2TestOnly))`.
    - Шесть ранее не покрытых общим состоянием boundary (`ButtonList`, `DropdownSelect`, `ListView`, `Modal`, `RichTextPopover`, `TabContainer`) получили тот же Designer-visible `PropertyHostState`. Sweep проверяет его struct type и metadata, round-trip двух разных `HostIdentity`, а для Screen Field host — делегирование `GetScreenFieldId()` в общую identity.
    - `WBP_Modal` через Unreal Editor API перепривязан к `UGV2ModalWidgetBase`, получил реальные `TitleText`, `ContentText`, `ButtonList` и `BackdropButton`, compiled и saved; благодаря этому он входит в обычный asset sweep, а не в ручное исключение.
    - Красный прогон до исправления зафиксировал отсутствие `PropertyHostState` у шести boundary и отсутствие real WBP instance у `UGV2ModalWidgetBase`; после восстановления `GV2.UI.CapabilityObservabilityCompositeSweep` зелёный как headless Editor automation, так и через `Tools/MCP/run_ue_tests.py` в запущенном Editor.
    - Контракты: `Docs/UI/WidgetRegistry.md` фиксирует reflection source set, обязательную fixture и identity checks; `Docs/UI/ScreenTemplates.md` фиксирует связь `HostIdentity` с sweep.

## Проверка milestone

- [x] Экран из базовых элементов работает без нового C++-класса.
- [x] Свойство идентичности одно и задаётся в Designer.
- [x] Новый хост с `key` не требует правки общего файла.
- [x] Хост, не попавший в sweep, невозможен.
