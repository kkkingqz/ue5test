---
title: Addressable Elements Tasks
status: active
version: 1.2
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

- [ ] **DUC-03 — `key` перестаёт быть перечислением классов**
  - `FGV2KeyPropertyConsumer::Commit` перечисляет 15 классов; 13 веток идентичны. Новый хост, объявивший `key`, обязан быть добавлен туда вручную — и до недавнего исправления его отсутствие давало тихий успех, что и произвело дефект `UGV2ModalWidgetBase`.
  - Done: ключ хранится в общем состоянии хоста свойств, consumer сводится к одной ветке; две семантически иные capability — выбранный ключ выпадающего списка и ключ вкладки по умолчанию — разведены по именам и не выдают себя за `key`; новый хост, объявивший `key`, работает **без** правки consumer — проверено тестом на классе, добавленном только в тесте; типизированный отказ на неподдерживаемый тип цели сохраняется как защита.
  - Evidence: `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, `Source/GV2/Public/UI/GV2UiPropertyHost.h`.

- [ ] **DUC-04 — Охват sweep следует за адресуемостью**
  - Зависимости: DUC-02, DUC-03.
  - Done: список классов, прогоняемых sweep наблюдаемости, выводится из фактического набора реализаций `IGV2UiPropertyHost`, а не перечисляется в тесте вручную; добавление хоста без покрытия sweep краснит сборку; проверка идентичности включена в sweep как обычная capability.
  - Evidence: `Source/GV2/Private/Tests/GV2UiCapabilityObservabilityTests.cpp`.

## Проверка milestone

- [ ] Экран из базовых элементов работает без нового C++-класса.
- [ ] Свойство идентичности одно и задаётся в Designer.
- [ ] Новый хост с `key` не требует правки общего файла.
- [ ] Хост, не попавший в sweep, невозможен.
