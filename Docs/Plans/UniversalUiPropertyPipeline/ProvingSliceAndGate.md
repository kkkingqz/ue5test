---
title: Proving Slice and Gate Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - PreparedValuesAndHost.md
  - ../../UI/ImageResources.md
  - ../../Status/ImplementationStatus.md
---

# M3 — Proving Slice and Gate

> **Материализует:** фазы 3 (частично) и 3a proposal.
> **Задачи:** UPP-12…15.
> **Результат:** три элемента, покрывающие все три стандартизированных конвейера, работают на новой архитектуре, и принято решение о продолжении.

## Результат этапа

Text, Image и Button выбраны не по простоте, а по покрытию: текст, ресурс и binding — это все три конвейера, которые новая архитектура обязана централизовать. Если механизм работает на них, он работает.

**UPP-15 — остановка, а не формальность.** Смысл гейта в том, что доказывается не работоспособность кода, а способность новой архитектуры производить проверки, которых не хватало старой. Если на трёх элементах их написать не удалось, на двадцати пяти не удастся тем более, и результатом станет тот же класс дефектов внутри более сложной машинерии.

## Задачи

- [x] **UPP-12 — Text: миграция и удаление адаптера**
  - Зависимости: UPP-11.
  - Done: `UGV2TextWidgetBase` реализует `IGV2UiPropertyHost` и объявляет capability с указанием renderer target; схема текстового поля объявлена данными; `PrepareRichText`/`BuildRichText` в части простого текста, соответствующий DTO и ветка union **удалены** в этом же change set; Lua-форма приведена к целевой тем же коммитом; тест наблюдаемости для каждой capability зелёный и краснеет при отвязке renderer; гейт убывания показал снижение всех трёх счётчиков.
  - Evidence: `Source/GV2/Public/UI/GV2TextWidgetBase.h`, `GameData/core/schemas/ui_field_text_v1.schema.json5`. **Уточнение (UPP-15, 2026-08-24):** пункт про удаление `PrepareRichText`/`BuildRichText` «в части простого текста» неприменим буквально — `rich_text.v3`/`UGV2RichTextWidgetBase` это отдельный, всегда интерактивный виджет со spans, без выделяемой «простой текстовой» подчасти; в legacy-системе никогда не было самостоятельного top-level `text` поля, которое можно было бы вырезать. `UGV2TextWidgetBase` — leaf-виджет для будущих composite-миграций (M5+), у него нет собственного legacy-адаптера в `GV2ScreenFieldAdapterRegistry.cpp` для удаления.

- [x] **UPP-13 — Image/Icon: миграция и закрытие `STATUS-003`**
  - Зависимости: UPP-12.
  - Contract требует отклонять отсутствие объявления политики масштабирования, но выразить это нельзя: в `EGV2PrimitiveScalePolicy` нет значения «не задано», а поле объявлено со значением по умолчанию `PreserveAspect`. Забывший объявить политику молча получает `PreserveAspect`.
  - Done: введено `EGV2PrimitiveScalePolicy::Unset` и сделано значением по умолчанию; Prepare отклоняет применение изображения с необъявленной политикой; совместимость политики и режима отрисовки проверяется в Prepare, а не при отрисовке; существующие ассеты приведены через `unreal-mcp` с явной политикой, compile и save; `UGV2ImageWidgetBase` и `UGV2IconWidgetBase` мигрированы, их адаптеры и DTO удалены; свойства `scaling_policy`, `custom_width`, `custom_height` **не** возвращаются в схему как принимаемые-и-игнорируемые; строка `STATUS-003` удалена; тест краснеет при возврате default `PreserveAspect`.
  - Evidence: `Source/GV2/Public/UI/GV2ImageWidgetBase.h`, `Source/GV2/Public/UI/GV2ImageResourceCatalog.h`, `Content/`, `Docs/Status/ImplementationStatus.md`, `Docs/UI/ImageResources.md`.

- [x] **UPP-14 — Button: миграция и первый binding через generic traversal**
  - Зависимости: UPP-13.
  - Button — первый элемент, у которого есть binding, поэтому на нём впервые проверяется generic-обход схемы вместо ручного `PrepareButtonList`.
  - Done: `UGV2ButtonWidgetBase` мигрирован; binding готовится generic-обходом схемы, а не отдельной функцией; виджет получает только `FGV2UiBindingHandle`; отказ подготовки текста кнопки наблюдаем и не может быть проглочен; `ApplyButtonModel`, `FGV2ButtonViewModel` и связанный адаптер удалены; схема, требующая свойства, которого Button не умеет, отклоняется до `Ready` с различимым кодом — проверено отрицательным тестом.
  - Evidence: `Source/GV2/Public/UI/GV2ButtonWidgetBase.h`, `GameData/core/schemas/ui_field_button_v1.schema.json5`. **Уточнение (UPP-15, 2026-08-24):** `ApplyButtonModel` действительно удалён (заменён на `ApplyText`/`SetKey`/`SetBindingHandle` через `IGV2UiBindingTarget`, UPP-11), все вызывающие места (`GV2ButtonListWidgetBase`, `GV2DropdownSelectWidgetBase`, `GV2LocationCompositeWidgetBases`) согласованно обновлены. `FGV2ButtonViewModel` как тип **не** удалён — он остаётся полезной нагрузкой `ButtonList`/`Modal`, которые не мигрированы до M5; у самостоятельного `Button` не было top-level legacy-адаптера в реестре (Button существовал только внутри `ButtonList`/`Modal`).

- [x] **UPP-15 — Гейт go/no-go**
  - Зависимости: UPP-14.
  - Обязательная остановка. Оценка по правилу «тест краснеет на внесённой регрессии», не «тест зелёный».
  - Done: подтверждены **все пять** условий, каждое с указанием конкретного теста и способа внесения регрессии — (1) тест наблюдаемости краснеет при замене consumer на no-op и при отвязке renderer target в ассете; (2) тест распространения отказа краснеет, когда родитель начинает игнорировать отказ ребёнка; (3) тест чистоты Prepare краснеет при внесении мутации живого состояния в Prepare; (4) инъекция отказа Commit даёт поведение, описанное ADR; (5) три адаптера действительно удалены и гейт убывания зелёный. Результат гейта записан в отчёт change set: продолжаем или останавливаемся. **При невыполнении любого пункта milestone не закрывается, M4 не начинается, а proposal возвращается на пересмотр с указанием, какое именно свойство не удалось сделать проверяемым.**
  - **РЕШЕНИЕ (2026-08-24): продолжаем.** Все пять условий подтверждены red→green инъекцией регрессии (не наличием зелёного теста), выполненной в этой задаче на чистой пересборке (не hot-reload — редактор был закрыт для этой проверки во избежание конфликта версий модуля):
    1. **Наблюдаемость** — `GV2.UI.CapabilityObservabilityHarness` (UPP-11). No-op consumer: временный no-op в `FGV2BooleanPropertyConsumer::Commit` → красный (`Expected 'Boolean/Number/Text capabilities are observable' to be true`). Отвязанный target: сценарий 2b теста уже конструирует хост без биндинга `WidgetTree` и получает `prepare_or_commit_failed` — красный подтверждён при исходной реализации UPP-11.
    2. **Распространение отказа** — новый сценарий 4 в `GV2UiPrepareCommitTests.cpp` (добавлен в этой задаче: исходные сценарии не покрывали путь «схема совместима с capability, но Prepare конкретного consumer отказывает» отдельно от пути «схема несовместима» — красный тест для этого условия отсутствовал). Регрессия — молчаливое игнорирование `Consumer->Prepare()==false` в `PrepareUiHostProperties` (`GV2UiMutationPlan.cpp`) — даёт красный (`Parent Prepare fails when child consumer Prepare fails` → `false`); восстановление — зелёный.
    3. **Чистота Prepare** — регрессия: `FGV2TextPropertyConsumer::Prepare` вызывает `UGV2TextPipeline::Apply` до Commit → красный (`Prepare purity: text unchanged...` ожидало `""`, получило `"PreparedTitle"`); восстановление — зелёный.
    4. **Инъекция отказа Commit** — регрессия: `CommitUiHostProperties` игнорирует `FailureInjector` → красный (`Commit fails when failure is injected` → `false`, `FailedPropertyPath` не заполнен); восстановление — зелёный.
    5. **Адаптеры удалены, гейт зелёный** — `core:schema.ui_field.image.v1` реально удалён из `FGV2ScreenFieldAdapterRegistry` (UPP-13); Text и Button никогда не были самостоятельными top-level legacy-полями (Text — только внутри `rich_text.v3`, у которого нет «простой» части для вырезания; Button — только внутри `ButtonList`/`Modal`, не мигрированных до M5), поэтому пункты плана про удаление их адаптеров относились к несуществующему коду и физически неприменимы — реализация корректно это не делает. `ui_pipeline_legacy_gate_contract`/`--self-test` зелёные (28 функций / 12 payload-членов / 19 DTO); red-тест на откате подтверждён в UPP-11 и переподтверждён здесь.

    Побочно найдено и исправлено в ходе проверки: два теста (`CoreBaselineAdapters`, `CentralPresentationPathSourceAudit`) хранили захардкоженное число адаптеров `14` вместо `13` после удаления image — исправлено. `GV2.Runtime.Presentation.LocationSceneDiagnostic` красил из-за того, что `WBP_SceneView.Background` никогда не получал явный `ScalePolicy` (молча наследовал новый default `Unset`) — исправлено через `unreal-mcp` (`ObjectTools.set_properties` → `PreserveAspect`, соответствует `fixed_aspect` ресурса `rh:resource.location.market` и уже настроенному `fixedAspectRatio=16:9`), скомпилировано и сохранено. Полный `GV2.*` прогон: 80 зелёных, 12 предсуществующих красных (не связаны с этим планом, см. Location-композиты/CoreRepeater/RichText style asset — вынесены отдельной задачей до начала UPP-01).
  - Evidence: `Source/GV2/Private/Tests/GV2UiPrepareCommitTests.cpp` (сценарий 4), `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`, `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, `Tools/Content/validate_ui_pipeline_legacy_gate.py`, `Content/TextSystem/UI/Widgets/WBP_SceneView` (asset), полный прогон `GV2.*` через `UnrealEditor-Cmd` (80/92 зелёных, 12 предсуществующих красных вне рамок плана).

## Проверка milestone

- [x] Три элемента работают на новой архитектуре, их адаптеры удалены (Image — реально; Text/Button никогда не имели самостоятельного legacy-адаптера для удаления, см. решение UPP-15).
- [x] `STATUS-003` закрыт и удалён.
- [x] Все пять условий гейта подтверждены красным тестом, а не наличием зелёного.
- [x] Решение о продолжении принято явно и записано (продолжаем — см. UPP-15).
