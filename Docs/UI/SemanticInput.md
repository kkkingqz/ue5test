---
title: Semantic Input Contract
status: draft
version: 1.4
updated: 2026-08-20
depends_on:
  - UIDocumentAndReconciliation.md
  - ../Architecture/CommandsAndEvents.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
---

# Semantic Input Contract

> **Владеет:** конвертом пользовательского ввода, binding handle и правилами отклонения устаревшего ввода.
> **Не владеет:** обработкой команды после её приёма ([Commands and Events](../Architecture/CommandsAndEvents.md)).
> **Инварианты:** [INV-007](../Architecture/Invariants.md)
> **Реализация:** `Source/GV2/Private/Bridge/GV2UiBindingRegistry.cpp`, `GV2RuntimeIngressQueue.cpp`, `Source/GV2/Private/UI/GV2UiInteractionEmitter.cpp`.
> **Проверки:** `GV2.Runtime.Ingress.*`, `GV2.Runtime.UI.BindingRegistry`.

Semantic input — value-only сообщение UE → Lua. Оно описывает смысл взаимодействия, а не Widget name, Blueprint callback или Lua function.

## Blueprint-facing ingress

Physical Widget публикует только control values и полученный при apply opaque handle:

```json5
{
  binding_handle: "runtime@17:91",
  input_values: {},
}
```

Blueprint-facing façade — `UGV2RuntimeSubsystem::SubmitUiInteraction(binding_handle, input_values)`. Первый vertical slice представляет `input_values` как `FGV2UiControlValue[]`: уникальное field name, scalar type и typed value. Для обычной кнопки массив пуст; text entry, slider и другие controls передают только поля своего declared input schema. Blueprint не передаёт `command_id`, bound args, session/revision identity или callback name.

Physical components вызывают façade только через общий component-side `FGV2UiInteractionEmitter`. Emitter централизует поиск active runtime, submit и technical result semantics. Прямой поиск `UGV2RuntimeSubsystem` в reusable button, RichText decorator или будущем control запрещён.

Метод возвращает `EGV2SubmitUiInteractionResult`: `Accepted`, `RuntimeNotReady`, `InvalidBindingHandle`, `StaleBindingHandle`, `InvalidInputValues` или `IngressQueueFull`. Rejection остаётся technical result C++ ingress и не создаёт gameplay Event.

Semantic Input Adapter резолвит handle в current UI binding registry, валидирует control fields и строит полный value-only envelope. Accepted item получает monotonically increasing `sequence` и помещается в coordinator-owned bounded FIFO ingress. Default capacity первого vertical slice — 256 items; capacity является private runtime configuration, а не Blueprint API.

## Runtime envelope

```json5
{
  session_generation: 17,
  ui_instance_id: "ui@17:8",
  revision: 42,
  sequence: 5,
  node_key_path: ["route", "main", "buttons", "travel_market"],
  element_id: "core:screen.main#widget.travel_market",
  command_id: "core:command.location.travel",
  args: {
    target_location_id: "core:location.city.market",
  },
}
```

`node_key_path` обязателен и берётся из binding record. Он является stable presentation path `layer → screen instance → field → item`, а не путём physical Widget tree. `element_id` optional и используется как authored provenance. `command_id` и bound part `args` также берутся только из record. Adapter добавляет разрешённые `input_values` по input schema; control values не могут перезаписать bound args.

## Validation order

1. Active session exists and is `Ready`.
2. `binding_handle` exists in active session registry.
3. Record `session_generation` is current.
4. Record `ui_instance_id` is current and input-enabled.
5. Record `revision` equals published interactive revision.
6. Record `node_key_path` still resolves to an interactive Screen Field element.
7. `input_values` содержат только unique allowed fields, включают все required fields, не пересекаются с bound args и проходят type/size/depth policy.
8. Построить immutable ingress item с next candidate `sequence` и попытаться enqueue в bounded FIFO.
9. Только после successful enqueue продвинуть accepted sequence и запустить fixed Lua Semantic Input entry point через non-reentrant pump.

Failure before step 9 never enters Lua command handler. Stale input и item, отклонённый из-за полной очереди, не replayed и не расходуют accepted sequence.

Если submit происходит во время обработки предыдущего ingress item, он может только enqueue новый item. Вложенный Lua/native sink call запрещён; внешний pump продолжает FIFO после возврата текущего handler. Session teardown сначала закрывает `Ready`, затем очищает queue и registry.

Текущий vertical slice преобразует UE envelope в STL-only DTO `GV2RuntimeCore::FSemanticInput` и вызывает fixed `game.runtime.dispatch_semantic_input`, установленный `core:module.boundary.ingress`. Lua function name не приходит из Blueprint/binding record и не является generic call API. Внутри Lua Semantic Input преобразуется в `CommandRequest` и сходится с direct headless ingress до Command Dispatcher. Dynamic Screen Element получает при apply только opaque handle. Host interaction sink вызывается только после успешного protected Lua call; Lua error переводит session в `Failed`, блокирует input и очищает registry/ingress.

При старте сессии стандартный пайплайн жизненного цикла открывает зарегистрированный экран (например, `WBP_Testscreen` для `core:screen.test`) со всеми элементами управления. Каждый Dynamic Screen Element (кнопки, чекбокс, поле ввода, выпадающий список) получает при apply только opaque `FGV2UiBindingHandle`; взаимодействие вызывает обычный `SubmitUiInteraction(handle, input_values)`. Lua dispatcher принимает команды по их canonical `command_id`, поэтому все handlers проверяются direct headless ingress-ом и не зависят от имени Widget, Blueprint event или Lua callback.

Clickable RichText span использует тот же механизм. Reconciler создаёт record с logical path `route → screen instance → rich_text field → span_id`; decorator получает только handle. Hover/unhover не создаёт Semantic Input. Click/keyboard activation вызывает `SubmitUiInteraction(handle, {})`, а bound scalar `args` span-а берутся только из record и не читаются из rich text tag.

Checkbox использует schema `core:schema.ui_input.checkbox_changed.v1`. Binding record объявляет единственное required control field `is_checked: boolean`; physical `WBP_Checkbox` отправляет `SubmitUiInteraction(handle, {is_checked})`. Значение является input, а не authoritative state: Lua command handler принимает его через Command Dispatcher и публикует новое desired checkbox state. Extra field, отсутствие `is_checked`, неверный type или collision с bound args возвращают `InvalidInputValues` до входа в Lua.

Input field (поле ввода) использует schema `core:schema.ui_input.text_changed.v1`. Binding record объявляет единственное required control field `value: string`; physical `WBP_InputField` отправляет `SubmitUiInteraction(handle, {value})` при фиксации или изменении текста (`OnTextCommitted`). Значение является input: Lua command handler принимает строку через Command Dispatcher и публикует новое desired text state. Extra field, отсутствие `value`, неверный type или collision с bound args возвращают `InvalidInputValues` до входа в Lua.

Dropdown использует schema `core:schema.ui_input.dropdown_selected.v1`. Binding record объявляет единственное required control field `selected_key: string`. Option button передаёт composite dropdown только deterministic local key и не отправляет interaction самостоятельно; `WBP_DropdownSelect` проверяет key против applied options и выполняет ровно один `SubmitUiInteraction(handle, {selected_key})`. Lua command handler принимает selection через Command Dispatcher и публикует новое desired state. Header open/close остаётся UE-local. Extra field, отсутствие `selected_key`, неверный type, неизвестный option key или collision с bound args отклоняются до изменения desired state.

Tab container controls (набор вкладок) строят binding records для всех вкладок сразу с удлинённым `node_key_path` вида `[layer, instance_key, field_id, tab_key, ...]`. Однако Semantic Input принимает handles только текущей активной вкладки контейнера. Handles неактивных вкладок отклоняются как `StaleBindingHandle`. Переключение вкладки является чисто UI-local операцией, изменяющей множество интерактивных handles без изменения ревизии документа и без отправки геймплейных команд.

## Security/authority boundary

- Blueprint не получает authoritative `command_id` и не может подменить binding registry record.
- Blueprint supplies only schema fields exposed by control contract; extra fields и collision с bound args отклоняются.
- Lua revalidates gameplay preconditions; enabled/visible UI state is not authority.
- Raw UObject, asset path, callback name и arbitrary event ID запрещены.

## Input ownership

Physical device state, hover, focus, pressed и repeated-key timing принадлежат UE input layer. Gameplay-significant selection/confirmation becomes semantic input.

RichText popover является UE-local transient surface: открытие, positioning, hover delay и закрытие не вызывают Lua. Если активация span должна открыть настоящий blocking Modal, click сначала проходит Command Dispatcher, после чего Lua публикует desired Screen Instance в `modal_stack`.

Modal input routing is resolved in Presentation before envelope creation. Only the top eligible modal or explicitly permitted layer can submit.

## Result routing

Command Result routes back using correlation ID and resolved current UI identity. Normal refusal может разместить localized error рядом со Screen Field element по `node_key_path`/`element_id` либо в configured surface. Если UI identity изменилась до результата, Presentation может отбросить local placement и оставить только global/system feedback.

## Technical input distinction

Async resource/save/platform completion uses runtime TechnicalInput, not SemanticInput and not gameplay EventBus. It contains operation ID/token-bound DTO and may enqueue a command after validation.

## Tests

Required tests: input-before-ready, unknown/stale handle, stale session/UI/revision, duplicate/out-of-order sequence, removed Screen Field item/span, fabricated handle, bound-arg collision, optional/required/extra control fields, malformed values, bounded-capacity rejection, FIFO nested submit without re-entry, debug start binding → Lua handler → presentation request, RichText span click с bound args, checkbox boolean input → desired state republish, input string → desired state republish, single dropdown `selected_key` submit → desired state republish, отсутствие hover ingress, modal blocking и late result placement.
