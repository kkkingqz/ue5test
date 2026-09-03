---
title: Prerequisites Tasks
status: active
version: 1.5
updated: 2026-09-03
depends_on:
  - README.md
  - ../../UI/WidgetRegistry.md
---

# M1 — Prerequisites

> **Материализует:** три отличия объявления от нынешних C++-композитов и мёртвую поверхность, найденную при разборе.
> **Задачи:** DCA-01…04.
> **Результат:** объявление выражает всё, что выражают три композита, а их ассеты приведены к тому, что модель уже умеет.

## Результат этапа

Три композита отличаются от объявляемого не набором видов — он совпадает полностью — а тремя механизмами: условным объявлением capability, косвенностью через внутренний репитер и трёхступенчатым откатом при выборе класса элемента. Этап снимает все три.

Четвёртая задача убирает поверхность, которая при разборе оказалась мёртвой и которую сводка `DUC-08` ошибочно назвала причиной невозможности миграции.

Задачи независимы и переставляются свободно.

## Задачи

- [x] **DCA-01 — Объявление выражает необязательное свойство**
  - Сегодня условность живёт в C++: `DescribeUiCapabilities` объявляет capability только при `if (Target != nullptr)`, поэтому один класс обслуживает варианты ассета, где, например, портрет отсутствует. Объявление же безусловно, и ссылка на непривязанного ребёнка становится отказом проверки экземпляра.
  - Инвариант: объявление описывает контракт, а не подстраивается под ассет. Условность, живущая в C++ (`if (Target != nullptr)`), делает одно и то же объявление означающим разное в разных ассетах — именно это и мешает выразить композит объявлением.
  - Не считается закрытием: трактовка любой непривязанной цели как необязательной; необязательность, выведенная из состояния ассета вместо записи объявления.
  - Done:
    - запись объявления получает признак необязательности, редактируемый в Designer;
    - необязательное свойство при непривязанном ребёнке не объявляется и не отказывает;
    - обязательное при непривязанном ребёнке отказывает до `Ready` с прежней типизированной диагностикой;
    - оба случая покрыты, положительный и отрицательный;
    - отсутствующее необязательное свойство остаётся отсутствующим полем схемы, а не появляется пустым;
    - контракт описывает, чем необязательность объявления отличается от необязательности поля схемы.
  - Evidence: `Source/GV2/Public/UI/GV2DeclaredCompositeWidgetBase.h`, `Docs/UI/ScreenTemplates.md`, `Source/GV2/Private/Tests/GV2DeclaredCompositeTests.cpp`.
  - **Реализация (2026-09-03):** `FGV2DeclaredUiCapability` получил `bOptional` (`bool`, default `false`, не зависит от `Kind`, editable в Designer). `UGV2DeclaredCompositeWidgetBase::DescribeUiCapabilities` перед добавлением записи проверяет `bOptional && ChildWidgetName != NAME_None && GetWidgetFromName(ChildWidgetName) == nullptr` — при выполнении всех трёх запись не добавляется в дерево вовсе (capability отсутствует для этой ревизии этого экземпляра целиком, а не появляется пустой). `bOptional == false` (умолчание, поведение до задачи) не меняется: неразрешённый `ChildWidgetName` доходит до switch как раньше, `PrepareUiHostProperties` отклоняет его тем же `core:diagnostic.ui_consumer.missing_target`. Значение читается только из явного флага — временная непривязанность ребёнка (например, ещё не сохранённый ассет) никогда сама по себе не делает запись необязательной.

    Различие с необязательностью поля схемы записано в `ScreenTemplates.md` (новый раздел «Необязательное свойство объявления (DCA-01)»): declaration-optional — структурное свойство экземпляра (существует ли child в `WidgetTree`, решается один раз, не зависит от ревизии значения); optional поле схемы — свойство значения (содержит ли конкретный payload значение для уже существующей capability). Они не требуют согласующего механизма: если объявленно-необязательная capability отсутствует у экземпляра, а schema всё равно требует её как `required`, это отклоняется тем же путём и тем же семейством диагностики, каким сегодня отклоняется schema, требующая любую другую отсутствующую у widget capability (`core:diagnostic.ui_capability.*`) — отдельный код не введён, поскольку это структурно тот же случай «schema шире capability». Отсутствующая capability не порождает mutation тем же существующим путём (`bSchemaOwns == false` в `PrepareUiHostProperties`), которым сегодня не порождает mutation любое не описанное в schema свойство — новый код для этого не понадобился.

    Новый тест `GV2.UI.DeclaredComposite.OptionalDeclaration`: (1) `bOptional=true` + непривязанный child → capability отсутствует в дереве (`FindProperty` возвращает `nullptr`), schema без этого поля готовится без ошибок и без mutation; (2) `bOptional=false` (default) + тот же непривязанный child → по-прежнему отклоняется `missing_target` — необязательность не обходит существующий контракт для обязательного свойства; (3) `bOptional=true` + привязанный child → capability присутствует, значение доходит до виджета через `Commit`. Red→green: временное отключение skip-check (`bOptional` игнорируется недоказуемым-компилятором `false`-guard'ом) уронило ровно сценарий (1) с сообщением «optional property with an unbound child is not declared at all» ожидало `null`, получило существующую capability; сценарий `GV2.UI.DeclaredComposite.ChildKindCompatibility` (не затронутый) остался зелёным. Восстановление — снова чисто.

    Верификация: 107/108 `GV2.*` UE Automation (headless, `-nullrhi`); единственный фейл — `GV2.Runtime.UI.NestedInstancesAndTabsContract` (сценарий с меткой `GBF-05`, незакоммиченная параллельная работа другой сессии над вложенным rollback в табах — не относится к `DCA-01`, не трогалось; вынесено отдельной задачей).

- [x] **DCA-02 — Хосты коллекций существуют в дереве виджетов**
  - Композиты принимают либо явный `UGV2ListViewWidgetBase`, либо голую панель (`MeterContainer`, `ItemIcons`, `EffectIcons`, `CharacterContainer`, `ButtonContainer`), и во втором случае создают репитер внутри себя. Объект transient, в `WidgetTree` его нет, адресация по имени до него не дотягивается — ради этого и написан мостик в планировщике.
  - Инвариант: адресуемое существует в дереве виджетов. Объект вне `WidgetTree` недостижим для адресации по имени, и обход этого требует специального кода под конкретный класс — того самого, который удаляется в `DCA-08`.
  - Не считается закрытием: обобщение поиска внутренних репитеров на любой хост — это закрепило бы адресацию объекта вне дерева как часть модели.
  - Done:
    - в `WBP_PlayerStatusPanel`, `WBP_SceneView` и `WBP_CommandPanel` каждая коллекция имеет привязанный `WBP_ListView` или эквивалентный виджет-репитер, присутствующий в `WidgetTree`;
    - голые панели как хосты коллекций не используются;
    - ассеты изменены через `unreal-mcp` с compile и save;
    - automation зелёная до удаления мостика: на этом шаге меняется форма ассета, а не поведение.
  - Evidence: `Content/TextSystem/UI/Widgets/`, вывод automation.
  - **Реализация (2026-09-03):** через `unreal-mcp` (UE Editor запущен агентом самостоятельно — mcp не был подключён на старте сессии, подключение восстановлено пользователем) созданы два новых reusable Widget Blueprint на `UGV2ListViewWidgetBase` с корневым `WrapBox` вместо `VerticalBox` у уже существовавшего `WBP_ListView`: `WBP_ListView_Wrap` (default dynamic wrap, `bExplicitWrapSize=false`) для `MeterRepeater`/`ItemRepeater`/`EffectRepeater`/`CharacterRepeater` и отдельный `WBP_ListView_WrapButtons` (`bExplicitWrapSize=true`, `WrapSize=1200`) для `ButtonRepeater`. Оба нужны, поскольку старый `ButtonContainer` уже был явно настроен вручную (`WrapSize=1200`) — единственный из пяти, для которого существует детальный geometry-тест (`LocationScreenViewportMatrix`, BAI-10); остальные четыре сохраняют dynamic-режим, каким они были прежде (без own теста, поведение не менялось).
    - В каждом из трёх composite-ассетов голая панель (`MeterContainer`/`ItemIcons`/`EffectIcons`/`CharacterContainer`/`ButtonContainer`) удалена и заменена на месте новым `WBP_ListView*` instance, переименованным в имя соответствующего `BindWidgetOptional`-репитера (`MeterRepeater` и т.д.) — auto-bind по имени сработал (`GetWidgets` подтвердил `bInherited: true` для всех пяти, старые панели-имена — `None`).
    - **Найденный при этом реальный Slate-баг**, не связанный с содержанием DCA-02, но обнаруженный им: `UWrapBox` с default `bExplicitWrapSize=false` (`bUseAllottedSize=true` внутри `SWrapBox`) обновляет `PreferredSize` только через `SWrapBox::Tick()`, вызываемый Slate-приложением по registered-tick списку. `SVirtualWindow`, которым пользуется `LocationScreenViewportMatrix` (и вообще любой headless/offline geometry-тест этого файла), никогда не регистрируется в `FSlateApplication` и поэтому никогда не тикает вложенные виджеты — `PreferredSize` застревает на значении с первого (самого широкого, 4K) прохода резолюций и никогда не уменьшается для узких. Ровно поэтому `ButtonContainer` был единственным из пяти с explicit `WrapSize` — прежний автор эмпирически обошёл эту же проблему для единственной коллекции, которую тестировал настолько детально. Значение `1200` восстановлено побайтовой сверкой со старым `.uasset` (`git show HEAD:.../WBP_CommandPanel.uasset`, временно загружен `unreal-mcp` во временную папку `/Game/_TempInspect`, `ObjectTools.get_properties` на `WidgetTree.ButtonContainer`, папка удалена после сверки).
    - Red→green: демонстрация — временная замена `ButtonRepeater`'s класса на `WBP_ListView_Wrap` (dynamic-режим) вместо `WBP_ListView_WrapButtons` красит именно `GV2.Runtime.UI.LocationScreenViewportMatrix` (BAI-10, buttons #4–6 переполняют bounds CommandPanel); возврат на `WBP_ListView_WrapButtons` — снова чисто.
    - `GV2.Runtime.UIKit.CentralThemeAndComponents`'s счётчик WBP-ассетов обновлён `33 → 35` (два новых production-ассета).
    - Верификация: portable ctest (`cmake-build-ci`) — 74/74; полный `GV2.*` UE Automation (live editor через `unreal-mcp`, после ребилда C++ `RunUBT.sh GV2Editor Linux Development`, `Result: Succeeded`) — 100/101, единственный фейл `GV2.Runtime.UI.NestedInstancesAndTabsContract` (`GBF-05`, чужая незакоммиченная работа, не относится к этой задаче); в частности `CentralThemeAndComponents` (WBP-счётчик 35) и `LocationScreenViewportMatrix` (wrap-регрессия) — зелёные.

- [x] **DCA-03 — Класс элемента задаётся явно**
  - `ResolveIconWidgetClass`, `ResolveMeterWidgetClass`, `ResolveCharacterWidgetClass` и `ResolveButtonWidgetClass` дают откат свойство экземпляра → CDO → `LoadClass` по фиксированному пути. Объявление несёт фиксированный `EntryWidgetClass`, и это правильнее: путь ассета, зашитый в C++, — скрытая зависимость кода от контента.
  - Этих четырёх мест недостаточно. Сканирование `Source/` даёт ещё два, вне композитов локации и потому переживающих `DCA-05…07`: `GV2ButtonListWidgetBase.cpp:74,79` и `GV2DropdownSelectWidgetBase.cpp:151,155` — оба `FindObject`/`LoadClass` по `/Game/UI/Widgets/WBP_Button.WBP_Button_C`. Это компоненты общего назначения, они не удаляются ни одной задачей плана, и без расширения области задача закрылась бы, оставив ровно ту зависимость, которую объявила снятой.
  - Инвариант: код не зависит от расположения контента. Зашитый в C++ путь ассета — скрытая зависимость обратного направления, которая переживает переименование молча.
  - Не считается закрытием: перенос отката в объявление как значения по умолчанию; сохранение `LoadClass` по фиксированному пути в качестве запасного варианта; закрытие по списку из шести известных мест — список известен только потому, что его составили сегодня, а перечислитель нужен для тех, кого добавят завтра.
  - Done:
    - класс элемента каждой коллекции задан явно там, где коллекция объявляется;
    - проверено, что ни одна из четырёх виртуальных функций никем не переопределена, и они удалены вместе с зашитыми путями;
    - `GV2ButtonListWidgetBase` и `GV2DropdownSelectWidgetBase` получают класс кнопки извне и не знают пути ассета;
    - отсутствие класса элемента у непустой коллекции остаётся отказом, а не подстановкой умолчания;
    - существует гейт, запрещающий литерал `/Game/` в production-коде `Source/` (тесты и фикстуры — вне области); множество берётся сканированием, не списком файлов;
    - гейт краснеет при внесении синтетического `LoadClass` по фиксированному пути — продемонстрировано.
  - Evidence: `Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp`, `Source/GV2/Private/UI/GV2ButtonListWidgetBase.cpp`, `Source/GV2/Private/UI/GV2DropdownSelectWidgetBase.cpp`, `Content/`, новый гейт.
  - **Реализация (2026-09-03):** все шесть `Resolve*WidgetClass` (четыре виртуальные на композитах локации + `UGV2ButtonListWidgetBase`/`UGV2DropdownSelectWidgetBase`) удалены целиком вместе с трёхступенчатым откатом (свойство экземпляра → CDO → `LoadClass` по фиксированному пути → `StaticClass()`). Взамен — тривиальные `UFUNCTION(BlueprintPure)` геттеры (`GetIconWidgetClass`, `GetMeterWidgetClass`, `GetCharacterWidgetClass`, `GetButtonWidgetClass`, `GetOptionWidgetClass`), читающие свойство как есть, без отката. `DescribeUiCapabilities` у всех пяти классов больше не оборачивает `AddKeyedCollection` в `if (Class)` — при `nullptr` capability всё равно объявляется (host существует), а `EntryWidgetClass` уходит в дерево пустым; существующий (не новый) generic-путь `FGV2KeyedCollectionPropertyConsumer::Prepare` уже отклоняет попытку создать НОВЫЙ элемент коллекции без класса кодом `core:diagnostic.ui_consumer.missing_entry_class` — переиспользован без изменений, ничего нового под это не писалось.

    `UGV2ModalWidgetBase::DescribeUiCapabilities` — особый случай: сохранён (не удалён) bare-native `UGV2ButtonWidgetBase::StaticClass()` как fallback ровно для случая «`ButtonList` не существует вовсе» (ортогональный вариант Modal без хоста кнопок, задокументированный существующим тестом `GV2.UI.StandardPropertyConsumers`, "Modal buttons entry class is ButtonWidgetBase"). Откат убран только из внутреннего случая — когда `ButtonList` существует, но его собственный класс не задан.

    Ассеты (через `unreal-mcp`, `ObjectTools.get_properties`/`set_properties` на `Default__<Class>_C`, `CompileWidgetBlueprint`, `save_assets`): `WBP_CommandPanel.buttonWidgetClass`, `WBP_ButtonList.buttonWidgetClass` и `WBP_PlayerStatusPanel.iconWidgetClass` уже были явно заданы в Designer до задачи (не зависели от отката). Довнесены недостающие: `WBP_PlayerStatusPanel.meterWidgetClass → WBP_ProgressBar`, `WBP_SceneView.characterWidgetClass → WBP_Icon`, `WBP_DropdownSelect.optionWidgetClass → WBP_Button` — те же классы, что раньше подставлял удалённый откат, поведение экрана локации не изменилось. `WBP_Modal`'s `ButtonList` — инстанс `WBP_ButtonList` (`bInherited: true`), наследует его `buttonWidgetClass` автоматически, отдельной правки не потребовал.

    Новый гейт `Tools/Testing/validate_no_hardcoded_asset_paths.py`: сканирует `Source/GV2/Public` и `Source/GV2/Private` (кроме `Tests/`) на литерал `"..../Game/...."` в строковых литералах; `--self-test` синтетически вносит нарушение во временный `Source/GV2/Public/*.h` (ловится) и в `Source/GV2/Private/Tests/*.cpp` (не ловится — подтверждает исключение тестов). Подключён в `CMakeLists.txt` (`no_hardcoded_asset_paths_contract` + `_negative_contract`), 76/76 portable ctest зелёные.

    Red→green (продемонстрировано дважды): (1) сам гейт — его `--self-test` уже содержит red/green цикл на синтетическом временном дереве; (2) рантайм-инвариант — временный возврат отката внутрь `UGV2LocationSceneWidgetBase::DescribeUiCapabilities` (`CharacterWidgetClass ?? UGV2ImageWidgetBase::StaticClass()`) уронил ровно `GV2.Runtime.Presentation.LocationCompositeUnresolvedClassRejection` («expected null, got non-null»); откат обратно — снова чисто.

    Побочная находка при аудите тестов: несколько существующих unit-тестов (`GV2PropertyConsumersTests.cpp`, `GV2UiPropertyHostTests.cpp`, `GV2UiCapabilityObservabilityTests.cpp`), создающих `ButtonList`/`Dropdown`/`PlayerStatus`/`Scene`/`CommandPanel` bare-native (не из Blueprint-ассета), молча полагались на удалённый откат для получения РЕАЛЬНОГО класса кнопки/иконки/прогресс-бара (нужного, чтобы generic `Prepare` смог разрешить `LabelText`/`Image`-таргеты внутри созданного элемента). Обновлены явным `LoadClass` реальных `WBP_Button`/`WBP_Icon`/`WBP_ProgressBar` через reflection — той же связкой имён ассетов, что раньше подставлял откат.

    Верификация: portable ctest (`cmake-build-ci`) — 76/76 (было 74, +2 новых теста гейта); полный `GV2.*` UE Automation (live editor через `unreal-mcp`, после ребилда `RunUBT.sh GV2Editor Linux Development`, `Result: Succeeded`) — 101/101, без единого фейла (ранее единственный известный фейл `NestedInstancesAndTabsContract`/`GBF-05` закрыт параллельной сессией до начала этой задачи).

- [ ] **DCA-04 — Мёртвая поверхность трёх композитов удалена**
  - `UGV2LocationCommandPanelWidgetBase::OnBindingInvoked` объявлен `BlueprintAssignable` и не транслируется ниоткуда: ноль `Broadcast` в реализации, ноль подписчиков в C++, ноль упоминаний в `WBP_CommandPanel`, `WBP_LocationScreen`, `WBP_GameShell`. `StaminaMeter` и `Character` помечены устаревшими и только сворачиваются.
  - Инвариант: объявленная поверхность используется либо отсутствует. `OnBindingInvoked` — третий найденный экземпляр этого семейства после `ResourceIcon` и `ApplyOptionalXxx`, и первый, который успел стать обоснованием ложного архитектурного вывода.
  - Не считается закрытием: пометка `DeprecatedProperty` вместо удаления; проверка ссылок только в трёх известных ассетах.
  - Done:
    - `OnBindingInvoked`, `StaminaMeter` и `Character` физически удалены из заголовка и реализации;
    - проверено, что ни один Blueprint на них не ссылается — поиск по всем `.uasset`;
    - ассеты, если ссылались, приведены через `unreal-mcp`;
    - гейт запрета legacy-поверхности расширен на эти имена.
  - Evidence: `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`, `Content/`, `Source/GV2/Private/Tests/`.

## Проверка milestone

- [ ] Необязательное свойство выразимо объявлением, обязательное при непривязанном ребёнке по-прежнему отказывает.
- [ ] Ни одна коллекция трёх композитов не адресует объект вне `WidgetTree`.
- [ ] Ни один класс элемента не подставляется зашитым в C++ путём ассета, и это утверждает сканирование, а не список.
- [ ] Три мёртвые сущности отсутствуют, и гейт не даёт им вернуться.
- [ ] Поведение экрана локации на этом этапе не изменилось.
