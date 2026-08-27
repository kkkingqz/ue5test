---
title: Confirmed Contract Gaps
status: informative
version: 2.4
updated: 2026-08-27
depends_on:
  - ../README.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../UI/PresentationSnapshotAndEffects.md
---

# Подтверждённые расхождения contract и реализации

> **Показывает:** только проверенные незакрытые gaps между нормативным contract и текущим кодом/tests.
> **Не является нормативным:** целевое поведение задаёт linked owner contract; Plans и Proposals определяют будущую работу.
> **Обновляется:** gap добавляется при подтверждённом расхождении и удаляется тем же change set, который его полностью закрывает.

Отсутствие строки не доказывает полноту реализации. Реализованные возможности здесь не перечисляются: их evidence находится в коде и tests. Roadmap, приоритеты и идеи в этот документ не входят.

## Состояния

- `missing` — обязательная contract surface отсутствует.
- `partial` — существует только часть обязательного lifecycle/behavior.
- `known_nonconformance` — реализация существует, но наблюдаемо нарушает конкретное правило.

## Открытые gaps

| ID | Состояние | Нормативное требование | Точное расхождение | Evidence |
|---|---|---|---|---|
| `STATUS-001` | `partial` | [Bootstrap and Session Lifecycle § Session states](../Architecture/BootstrapAndSessionLifecycle.md#session-states), [§ Replacement sequences](../Architecture/BootstrapAndSessionLifecycle.md#replacement-sequences) | Реализован cold start `NewGame`/`LoadSave`, но coordinator не проводит session через `Registering`, `BuildingState`, `RestoringInstances`, `Starting`, `PreparingPresentation`; отсутствуют active-session preflight, cancellation и Menu↔Game/load-another-save/content-reload replacement flow. | `FGV2SessionCoordinator::StartSession` в `Source/GV2/Private/Application/GV2SessionCoordinator.cpp` выставляет `Creating`, затем сразу `Ready`; `FRuntimeSession::StartFromSave` покрыт `GV2ColdStartLoadConformance`, но production entry point replacement operation отсутствует. |
| `STATUS-002` | `missing` | [Presentation Snapshot and Effects § Effect](../UI/PresentationSnapshotAndEffects.md#effect), [§ ordering](../UI/PresentationSnapshotAndEffects.md#snapshoteffect-ordering) | UI document/reconciliation реализованы, но public one-shot effect DTO/queue/apply path, stale target handling и effect non-persistence tests отсутствуют. | В `Scripts/`, `Source/` и `Tests/` нет production `publish_effect`/effect queue consumer; `Scripts/authoring/presentation.lua` публикует только desired UI document. |
| `STATUS-005` | `partial` | [Universal UI Property Pipeline README § Итоговый Definition of Done](../Plans/UniversalUiPropertyPipeline/README.md#итоговый-definition-of-done) ("Объявленная capability... сборка краснеет"), [TeardownAndClosure § UPP-32](../Plans/UniversalUiPropertyPipeline/TeardownAndClosure.md#задачи) | `RunUiCapabilityObservabilityHarness` реально вызывается только для 10 из 18 классов, реализующих `IGV2UiPropertyHost` (`GV2UiCapabilityObservabilityTests.cpp`: Text/Image/Icon/Button/Checkbox/InputField/ProgressBar/Portrait/RichText/RichTextPopover). Собственные (не delegated в `CollectionHost`/`NestedScreen`) leaf-capability не прогнаны через harness для: `UGV2DropdownSelectWidgetBase` (`placeholder`, `selected_key`, `binding`, `is_open`), `UGV2ModalWidgetBase` (`title`, `content`, `backdrop_close_action`, `key`), `UGV2TabContainerWidgetBase` (`default_tab_key`, `key`), четырёх Location-композитов (`day`/`location`/`primary_resource`/`key` на `LocationTopBar`; `name`/`portrait_resource_id`/`key` на `LocationPlayerStatus`; `background_tile_resource_id`/`background_resource_id`/`context_text`/`key` на `LocationScene`; `key` на `LocationCommandPanel`). Эти capability реально применяются (есть отдельные функциональные тесты в `GV2PropertyConsumersTests.cpp`), но не прошли именно генерический observability-sweep. `UGV2ButtonListWidgetBase`/`UGV2ListViewWidgetBase` расхождением не являются — у них нет ни одной собственной `RendererControl` capability, только `CollectionHost`, который harness намеренно пропускает (см. обоснование в `GV2UiCapabilityObservability.cpp:318-323`). | Обнаружено сверкой UPP-32 (2026-08-27): `grep -rn RunUiCapabilityObservabilityHarness Source/GV2` — единственный вызывающий файл `GV2UiCapabilityObservabilityTests.cpp`, инстанцирующий ровно 10 из 18 классов. Cast-based state capture для `key` уже существует в `GV2UiCapabilityObservability.cpp` для 4 Location-композитов (написан заранее, но неиспользован в тестах); для Modal/Dropdown/TabContainer capture ещё не написан. |

## Правило изменения

- Новый contract и полностью соответствующая реализация не создают строку.
- Частичная реализация создаёт или уточняет строку со ссылкой на точное правило и code/test evidence.
- Полное закрытие удаляет строку; история остаётся в commit и выполненном Plan summary.
- Предположение без проверки кода/tests сюда не добавляется.
