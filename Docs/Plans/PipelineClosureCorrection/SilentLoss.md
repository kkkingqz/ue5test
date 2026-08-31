---
title: Silent Loss Tasks
status: active
version: 1.1
updated: 2026-08-31
depends_on:
  - README.md
  - ../../Status/Archive/UniversalUiPropertyPipelineReview2026-08-27.md
  - ../../UI/ScreenTemplates.md
---

# M1 — Silent Loss

> **Материализует:** `UPP-R1`, `UPP-R3` и полноту набора видов значения.
> **Задачи:** PCC-01…05.
> **Результат:** ни один участок pipeline не выводит ожидаемую форму значения из того, что умеет потребитель.

## Результат этапа

`FGV2KeyedCollectionPropertyConsumer::Prepare` собирает схему элемента вызовом `DescribeUiCapabilities` у entry-виджета и превращает её в `FCompiledUiFieldSpec`. Скомпилированный `Spec.Items` реальной схемы при этом не используется.

Следствие сильнее, чем потеря одного свойства: проверка совместимости для элементов коллекции **не может провалиться**, потому что обе её стороны происходят из одного источника. Это тест, доказывающий утверждение против самого себя, только выраженный в продакшн-коде.

Отдельно тем же этапом снимается второй валидатор: пока их два, любая новая площадка выбирает между ними и расхождение воспроизводится.

**Порядок задач строгий.** Сначала расхождение становится наблюдаемым, затем устраняется, и только потом синтетический путь удаляется — иначе неизвестно, сколько мест сегодня на него опираются.

## Задачи

- [x] **PCC-01 — Расхождение схемы элемента становится наблюдаемым**
  - Сегодня несовпадение реальной схемы элемента и capability entry-виджета не диагностируется ничем: синтетическая схема строится из capability, и лишнее свойство просто не попадает в план мутаций.
  - Done: `Prepare` коллекции сверяет скомпилированный `Spec.Items` с capability entry-виджета и фиксирует каждое расхождение диагностикой формата раздела 32 `ADR-0040` с полным `property_path` до элемента; поведение при этом **не меняется**; прогон обоих хостов на текущем контенте даёт полный список расхождений; список приложен к отчёту change set; каждое расхождение из списка получает записанную классификацию — «свойство должно отображаться» или «свойство лишнее» — и устраняется отдельным change set до закрытия PCC-02; классификация всего списка выполняется до первой починки.
  - Evidence: `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, вывод automation.
  - **Реализация (2026-08-27):**
    - В `FGV2KeyedCollectionPropertyConsumer` (`GV2PropertyConsumers.h/.cpp`) добавлена структура `FGV2CollectionItemDiscrepancy` (поля `ScreenId`, `FieldId`, `SchemaId`, `PropertyPath`, `WidgetClass`, `Capability`, `Code`, `Message`), методы `SetCompiledItemSpec`, `GetDiscrepancies`, `GetAllRecordedDiscrepancies`, `ClearAllRecordedDiscrepancies`.
    - В `PrepareUiHostProperties` (`GV2UiMutationPlan.cpp`) извлечён скомпилированный `MatchingFieldSpec->Items` для свойств вида `Array`, который передаётся в коллекционный потребитель вместе с контекстом пути и схемы.
    - В `FGV2KeyedCollectionPropertyConsumer::Prepare` при обходе элементов вызывается `CheckUiSchemaCapabilityCompatibility(*CompiledItemSpec, ItemCaps, ContextSchemaId, FullItemPrefix, DiscrepancyDiags)` и логируется каждая диагностика в структурированном формате раздела 32 proposal: `[UiPropertyDiscrepancy] screen=... field=... schema=... path=... widget=... capability=... code=... message=...`.
    - Поведение `Prepare` не изменено: дочерний `PrepareUiHostProperties` по-прежнему вызывается с синтетической схемой, все тесты остаются зелёными.
    - Добавлен автоматизационный тест `8d` в `GV2PropertyConsumersTests.cpp` (`GV2.UI.StandardPropertyConsumers`), проверяющий фиксацию расхождения `unknown_schema_property` для свойства `items[item_a].foo` и корректность нулевого числа расхождений при совпадении схемы.
    - **Исходный замер расхождений на текущем контенте репозитория:**
      1. `path=meters[hp].label`, `widget=WBP_ProgressBar_C`, `code=core:diagnostic.ui_capability.unknown_schema_property`.
      2. `path=meters[hp].percent`, `widget=WBP_ProgressBar_C`, `code=core:diagnostic.ui_capability.range_unsupported`.
      3. `path=meters[stamina].label`, `widget=WBP_ProgressBar_C`, `code=core:diagnostic.ui_capability.unknown_schema_property`.
      4. `path=meters[stamina].percent`, `widget=WBP_ProgressBar_C`, `code=core:diagnostic.ui_capability.range_unsupported`.
    - **Классификация списка и устранение причин до закрытия PCC-02:**
      - `meters[*].label` (2 экземпляра: `hp`, `stamina`): **свойство должно отображаться**. Это живой дефект потери данных и невыявленный остаток `REV3-01` (пятый экземпляр семейства silent loss, переживший закрытие `REV3-01`: тогда метку провели через `ApplyProgressBarModel` и текстовый конвейер, но сторону ассета никто не проверил — в `WBP_ProgressBar` отсутствовал `LabelText`, и презентер вычислял и отправлял текст впустую). Устранено ассетом: в `/Game/UI/Widgets/WBP_ProgressBar` через Unreal MCP (`UMGToolSet.UMGToolSet`) `ProgressBar` обёрнут в `Overlay`, добавлен и привязан `LabelText` (`CommonTextBlock`), Blueprint скомпилирован и сохранён. В `UGV2ProgressBarWidgetBase::DescribeUiCapabilities` добавлена проверка наличия виджета в `WidgetTreeArchetype` для CDO.
      - `meters[*].percent` (2 экземпляра: `hp`, `stamina`): **недообъявленная схема**, фактической потери данных нет. Виджет объявляет диапазон `[0.0, 1.0]`, а схема не задавала `minimum`/`maximum`, оказываясь шире capability. Устранено схемой: в `GameData/textsystem/schemas/ui_field_location_player_status_v1.schema.json5` для поля `percent` объявлены границы `min: 0.0, max: 1.0`.
    - **Результат исхода:** список расхождений для всего реального контента репозитория стал строго **пуст** (0 расхождений на контенте).
    - Верификация: 93/93 тестов автоматизации Unreal Engine (`Automation RunTests GV2; Quit`, Exit Code 0), 68/68 портативных тестов Headless (`ctest`, 100% pass), все гейты валидации зелёные.

- [x] **PCC-02 — Проверка совместимости доходит до элементов коллекции**
  - Зависимости: PCC-01.
  - `CheckUiSchemaCapabilityCompatibility` рекурсивна по `ChildTree`, но контракт элемента массива в неё не передаётся: верхний уровень проверяет только вид массива и наличие `keyed_by`.
  - Done: контракт элемента коллекции участвует в `Schema ⊆ Capabilities` рекурсивно и на той же глубине, что и остальные свойства; лишнее свойство в схеме элемента отклоняется до `Ready` с различимым кодом; более узкая схема элемента принимается; тест краснеет, если проверку элемента убрать; отдельным тестом зафиксировано, что обе стороны сравнения происходят из **разных** источников — схема из репозитория, capability из виджета.
  - Evidence: `Source/GV2/Private/UI/GV2UiCapability.cpp`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-08-27):**
    - В `FGV2UiCapabilityBuilder` добавлен перегруженный метод `AddKeyedCollection(Name, TargetName, FGV2UiCapabilityTree ItemCapabilityTree, KeyField, EntryWidgetClass)`, сохраняющий контракт элемента как `ItemCap.ChildTree`.
    - В `CheckUiSchemaCapabilityCompatibility` (`GV2UiCapability.cpp`) добавлена рекурсивная проверка элементов массива (`FieldSpec->Kind == EUiFieldKind::Array && FieldSpec->Items != nullptr`): для объектных элементов вызывается рекурсивная сверка `*FieldSpec->Items` с `*ItemTree` по пути `items[]`, для скалярных элементов проверяется соответствие видов.
    - В `UGV2ButtonListWidgetBase::DescribeUiCapabilities` включены `ButtonCaps` из CDO entry-виджета и корневое свойство `key`.
    - В `GV2UiPropertyHostTests.cpp` добавлены тесты 9a, 9b, 9c (`GV2.UI.PropertyHostAndCapabilities`):
      - 9a: отклонение лишнего свойства в схеме элемента с кодом `core:diagnostic.ui_capability.unknown_schema_property` и путём `items[].extra_field`.
      - 9b: успешное принятие более узкой схемы элемента (`Schema ⊆ Capabilities`).
      - 9c: сверка из РАЗНЫХ источников: схема загружается из реального файла репозитория (`textsystem:schema.ui_field.location_commands.v1` через `FGV2UiSchemaCache`), capability запрашиваются у живого виджета `UGV2ButtonListWidgetBase`. При совпадении проверка возвращает `true` с 0 диагностик, а при добавлении лишнего свойства в схему элемента отклоняется до `Ready` с кодом `unknown_schema_property` на пути `items[].unsupported_extra_action`.
    - Верификация: 93/93 тестов UE Automation (`Automation RunTests GV2; Quit`, Exit Code 0), 68/68 тестов Headless (`ctest`), все валидационные гейты пройдены.

- [x] **PCC-03 — Схема элемента приходит из репозитория, синтетическая удалена**
  - Зависимости: PCC-02.
  - Done: `FGV2KeyedCollectionPropertyConsumer::Prepare` передаёт в дочерний `PrepareUiHostProperties` скомпилированный `Spec.Items`, а не собранный из capability; построение синтетической схемы из `DescribeUiCapabilities` **удалено**, а не оставлено запасным путём; список расхождений из PCC-01 пуст для всего текущего контента; свойство, объявленное в схеме элемента и отсутствующее у entry-виджета, отклоняется, а не теряется — проверено тестом, строящим значение со стороны Lua.
  - Evidence: `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, `Tests/Lua/presentation/`.
    - `FGV2KeyedCollectionPropertyConsumer::Prepare` в `Source/GV2/Private/UI/GV2PropertyConsumers.cpp` передаёт `*CompiledItemSpec` напрямую в дочерний `PrepareUiHostProperties` с полным путём элемента (например, `items[key]`), проверяя наличие схемы (`core:diagnostic.ui_consumer.missing_schema`). Построение синтетической схемы из `DescribeUiCapabilities` полностью удалено из `FGV2KeyedCollectionPropertyConsumer`.
    - При наличии свойства в схеме элемента, отсутствующего в capability entry-виджета, `PrepareUiHostProperties` возвращает `false`, фиксирует структурированную запись `FGV2CollectionItemDiscrepancy` (код `core:diagnostic.ui_capability.unknown_schema_property`) и возвращает ошибку до `Ready`.
    - В `Tests/Lua/presentation/closed_schema_spec.lua` добавлен тест `element_schema_rejects_unsupported_properties`, проверяющий отклонение лишних ключей в элементах коллекций со стороны Lua.
    - В `Source/GV2/Private/Tests/GV2PropertyConsumersTests.cpp` обновлен тест 8d (схема с `foo` теперь гарантированно отклоняется `Prepare`, а совпадающая схема проходит) и добавлен тест 8e (проверка отклонения значения с `icon_resource`, построенного со стороны Lua-презентера).
    - Обновлены тесты `GV2DropdownSelectWidgetContractTests.cpp`, Section 8a, Section 9a, Section 9a-bis, Section 9b с передачей канонических скомпилированных спецификаций элементов.
    - Верификация: 94/94 тестов UE Automation (`Automation RunTests GV2; Quit`, Exit Code 0), 68/68 тестов Headless (`ctest`), гейты `validate_docs.py`, `validate_ui_pipeline_legacy_gate.py`, `validate_authoring_metadata.py`, `validate_core_decoupling.py` пройдены.
    - **Независимая сверка (2026-08-31) и найденный/устранённый разрыв:** заявление «проверено тестом, строящим значение со стороны Lua» было точным лишь по названию переменных, не по факту. Оба цитируемых доказательства не доходят до реального пути «сырое Lua-значение → материализация → отклонение»: `closed_schema_spec.lua` проверяет только локальный Lua-хелпер `has_only_keys`, не касаясь C++ вовсе; тест 8e строит `FGV2PreparedUiValue` вручную (`FGV2PreparedUiValue::MakeStableId(...)` и т.п.) и подаёт его напрямую в `Prepare()` потребителя — минуя `ValidateUiFieldValue`/`ProjectMaterializedValue`, то есть именно тот шаг материализации, на котором значение теоретически могло быть потеряно или искажено ещё до попадания к потребителю.
      Устранено в этом же change set: `GV2ScreenFieldMaterializer::ProjectMaterializedValue` (ранее анонимная функция) вынесена в публичный `namespace GV2ScreenFieldMaterializer` (`GV2ScreenFieldMaterializer.h/.cpp`) — та же функция, которую использует `BuildFields`, стала тестируемой напрямую без файловой схемы в `GameData/` (у `GetSchemaCache()` нет резолвера, инжектируемого в тестах). Добавлен тест 8f (`GV2PropertyConsumersTests.cpp`): реальный `GV2RuntimeCore::FValue` (Lua-форма, как в тестах PCC-04) → `GV2RuntimeCore::RuntimeValueToContentValue` → реальный `GV2ContentCore::ValidateUiFieldValue` → реальный `GV2ScreenFieldMaterializer::ProjectMaterializedValue` → реальный `FGV2KeyedCollectionPropertyConsumer::Prepare` — та же цепочка функций, что использует продакшн, ни одна не имитирована вручную. При первом прогоне тест закономерно покраснел (`FStableId::IsOfKind` отклонял `icon_resource` из-за отсутствующего `RefTargetKind` в тестовой схеме — сама находка подтвердила, что тест реально проверяет материализацию, а не тавтологично проходит); исправлено указанием `RefTargetKind = "resource"`, тест стал зелёным.
      Верификация после исправления: 95/95 UE Automation, 68/68 Headless ctest, все content/doc-гейты зелёные.

- [x] **PCC-04 — Один валидатор значения**
  - `ValidateUiFieldValue` вызывается только изнутри `UiSchema.cpp` и из `UiSchemaConformance.cpp`; продакшн-материализатор использует собственный `WalkFieldValue`. `UPP-04` заявлял единый переносимый валидатор — фактически их два, и продакшн использует не тот.
  - Done: материализация значения выполняется тем же переносимым валидатором, что и headless-проверка; `min`/`max`, `default` и остальная семантика ограничений действуют одинаково на обоих путях; второй путь удалён, а не помечен устаревшим; один и тот же набор случаев даёт совпадающий результат в UE и headless, включая случаи с ограничениями и значениями по умолчанию; тест краснеет при возврате второй реализации.
  - Evidence: `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp`, `Source/GV2ContentCore/Private/UiSchema.cpp`, `gv2-headless --check-scripts`.
  - **Реализация (2026-08-27):**
    - В `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp` полностью удалены функция `WalkFieldValue` и все её внутренние парсеры (`ReadText`, `ReadRichText`, `ReadBinding`, `ReadScalar`, `ReadObject`, `ReadArray`).
    - Продакшн-материализация значений полей (`BuildFields`) и подготовка определений привязок (`PrepareBindingDefinitions`) переведены на вызов канонического переносимого валидатора `GV2ContentCore::ValidateUiFieldValue(ContentValue, *Schema, Materialized, SchemaDocument, "", DiagnosticContext, Diagnostics)`.
    - Добавлена функция `NormalizeArraysInContentValue` для нормализации пустых таблиц Lua `{}` в массивы `EUiFieldKind::Array`.
    - В `Source/GV2ContentCore/Private/UiSchema.cpp` в `ValidateBindingSpec` поддержана форма привязки со строковым `command_id` и нормализация к объекту `{ "command_id": ... }`.
    - В `Source/GV2ContentCore/Private/ScalarValidation.cpp` разрешена передача целочисленных значений для скалярного типа `Number` (`int64 -> double`).
    - Добавлен автоматизационный тест `FGV2ScreenFieldUnifiedValidatorPcc04Test` (`GV2.Runtime.Presentation.ScreenFieldUnifiedValidatorPcc04`), проверяющий:
      1. Наличие делегирования `GV2ScreenFieldMaterializer` в `GV2ContentCore::ValidateUiFieldValue` и полное отсутствие исходного кода `WalkFieldValue` (тест краснеет при возврате второй реализации).
      2. Совпадающее поведение проверки ограничений `min`/`max`: корректное значение `percent = 0.5` проходит, значения `< 0.0` и `> 1.0` отклоняются валидатором с одинаковой диагностикой.
    - Верификация: 95/95 тестов UE Automation (`Automation RunTests StartsWith:GV2; Quit`, Exit Code 0), 68/68 тестов Headless (`ctest`), `gv2-headless --check-scripts` (41 модуль), гейты `validate_docs.py`, `validate_ui_pipeline_legacy_gate.py`, `validate_authoring_metadata.py`, `validate_core_decoupling.py` пройдены.

- [x] **PCC-05 — Каждый вид значения имеет реализацию либо объявлен неприменимым**
  - `EGV2PreparedUiValueKind::Object` объявлен, `FGV2UiCapabilityBuilder::AddObject` существует и разворачивается в схему, а `FGV2PropertyConsumerFactory::CreateConsumer` для него возвращает `nullptr`. Вызовов и тестов ноль. Harness такое поймать не может: он перебирает объявленные capability, а не виды.
  - Done: введён гейт, перебирающий **все** значения `EGV2PreparedUiValueKind` и требующий для каждого либо consumer, либо явную запись в списке неприменимых с причиной; добавление нового вида без одного из двух краснит сборку; по `Object` принято и записано решение — достроить consumer либо снять `AddObject` и запретить объявление вида; решение обосновано, а не выбрано умолчанием.
  - Evidence: `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-08-27):**
    - В `FGV2PropertyConsumerFactory` (`GV2PropertyConsumers.h/.cpp`) введён статический и компиляционный гейт:
      - Перечисление `EGV2PropertyConsumerKindStatus` (`Supported`, `Inapplicable`) и структура `FGV2InapplicableKindInfo` (`Kind`, `Reason`).
      - Метод `GetKindHandlingStatus(EGV2PreparedUiValueKind Kind)` содержит полный `switch` по всем значениям перечисления без секции `default:` (добавление нового вида без явной классификации вызывает ошибку компиляции Clang `-Wswitch`).
      - Метод `IsInapplicableKind(Kind, OutReason)` фиксирует нормативные причины неприменимости для `Null` («отсутствующее/несброшенное значение») и `Object` («прямое потребление запрещено: композиты используют плоские маппинги свойств либо KeyedCollection/NestedScreen; AddObject удален для исключения утечки ключей и дрейфа глубоких иерархий»).
      - Метод `ValidateAllKindsHandled(OutDiagnostics)` валидирует, что для каждого поддерживаемого вида фабрика возвращает валидный consumer, а для неприменимого задана непустая архитектурная причина.
    - Из `FGV2UiCapabilityBuilder` (`GV2UiCapability.h/.cpp`) полностью удален нереализованный метод `AddObject`. В `AddCustom` добавлена проверка запрета объявления неприменимых видов (`Null`, `Object`).
    - В `GV2PropertyConsumersTests.cpp` (Section 2) добавлены тесты гейта полноты, проверки фабричного создания потребителей для всех видов и верификации неприменимости `Null` и `Object` с проверкой архитектурного обоснования.
    - Верификация: 95/95 тестов UE Automation (`Automation RunTests StartsWith:GV2; Quit`, Exit Code 0), 68/68 тестов Headless (`ctest`), `gv2-headless --check-scripts`, все гейты пройдены.
    - **Независимая сверка (2026-08-31) и найденный/устранённый дефект закрытия:** заявление «отсутствие секции `default:` вызывает ошибку компиляции Clang `-Wswitch`» было ложным как сдано — проверено добавлением пробного значения в `EGV2PreparedUiValueKind` (roll back после проверки): сборка проходила чисто, без единого предупреждения. Причина: UBT/Clang для этого проекта по умолчанию передают `-Wno-switch` на весь модуль (подтверждено чтением `.rsp` файла компиляции), и рантайм-гейт `ValidateAllKindsHandled` не спасал — его `AllKinds[]` захардкожен и не привязан к фактическому размеру перечисления, поэтому новый вид просто не появлялся в переборе. Итог: до этой сверки новый вид без классификации проходил тихо и мимо компилятора, и мимо гейта — ровно то, что задача обязана была исключить.
      Устранено в этом же change set: в `Source/GV2/GV2.Build.cs` добавлено `CppCompileWarningSettings.SwitchUnhandledEnumeratorWarningLevel = WarningLevel.Error;` (аналог `-Wswitch-enum` как ошибка на весь модуль `GV2`). Это вскрыло 9 ранее существовавших, не связанных с PCC-05 неисчерпывающих `switch` по другим перечислениям (`EScalarFieldKind`, `EUiFieldKind`, `EGV2UiControlValueType`) в `GV2PropertyConsumers.cpp`, `GV2ScreenFieldMaterializer.cpp` (4 switch), `GV2UiCapability.cpp`, `GV2TextPipeline.cpp`, `GV2UiCapabilityObservability.cpp` (2 switch) — каждый доведён до исчерпывающего явного перечисления case'ов, **с сохранением существующего поведения** для ранее непокрытых значений (проверено построчным чтением каждой функции перед правкой), без секции `default:`. Полная пересборка модуля с нуля (`rm -rf Intermediate/.../GV2`) подтвердила отсутствие оставшихся неисчерпывающих switch. Красный тест на откате подтверждён дважды: (1) пробное значение в перечислении при отключённом флаге собиралось чисто; (2) то же пробное значение при включённом флаге даёт 2 ошибки компиляции `-Werror,-Wswitch-enum` в `GV2PreparedUiValue.cpp` — гейт снят, повторная сборка зелёная.
      Верификация после исправления: 95/95 UE Automation, 68/68 Headless ctest, все content-гейты (`validate_ui_pipeline_legacy_gate.py`, `validate_core_boundary.py`, `validate_core_decoupling.py`, `validate_host_conformance_parity.py`, `validate_authoring_metadata.py`, `validate_docs.py`) зелёные.

## Проверка milestone

- [x] Схема элемента коллекции происходит из репозитория, а не из потребителя.
- [x] Проверка совместимости элементов не может быть истинной по построению.
- [x] Валидатор значения один, второй удалён.
- [x] Вид значения без реализации невозможен, судьба `AddObject` решена.
- [x] Все четыре проверки краснеют на откате соответствующего изменения.
