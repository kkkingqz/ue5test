---
title: Semantic Input Contract
status: draft
version: 0.6
updated: 2026-08-10
depends_on:
  - UIDocumentAndReconciliation.md
  - ../Architecture/CommandsAndEvents.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
---

# Semantic Input Contract

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

Метод возвращает `EGV2SubmitUiInteractionResult`: `Accepted`, `RuntimeNotReady`, `InvalidBindingHandle`, `StaleBindingHandle`, `InvalidInputValues` или `IngressQueueFull`. Rejection остаётся technical result C++ ingress и не создаёт gameplay Event.

Semantic Input Adapter резолвит handle в current UI binding registry, валидирует control fields и строит полный value-only envelope. Accepted item получает monotonically increasing `sequence` и помещается в coordinator-owned bounded FIFO ingress. Default capacity первого vertical slice — 256 items; capacity является private runtime configuration, а не Blueprint API.

## Runtime envelope

```json5
{
  session_generation: 17,
  ui_instance_id: "ui@17:8",
  revision: 42,
  sequence: 5,
  node_key_path: ["route", "travel_market"],
  element_id: "core:screen.city_map#widget.travel_market",
  command_id: "core:command.location.travel",
  args: {
    target_location_id: "core:location.city.market",
  },
}
```

`node_key_path` обязателен и берётся из binding record. `element_id` optional и используется как authored provenance. `command_id` и bound part `args` также берутся только из record. Adapter добавляет разрешённые `input_values` по input schema; control values не могут перезаписать bound args.

## Validation order

1. Active session exists and is `Ready`.
2. `binding_handle` exists in active session registry.
3. Record `session_generation` is current.
4. Record `ui_instance_id` is current and input-enabled.
5. Record `revision` equals published interactive revision.
6. Record `node_key_path` still resolves to an interactive node.
7. `input_values` содержат только unique allowed fields, включают все required fields, не пересекаются с bound args и проходят type/size/depth policy.
8. Построить immutable ingress item с next candidate `sequence` и попытаться enqueue в bounded FIFO.
9. Только после successful enqueue продвинуть accepted sequence и запустить fixed Lua Semantic Input entry point через non-reentrant pump.

Failure before step 9 never enters Lua command handler. Stale input и item, отклонённый из-за полной очереди, не replayed и не расходуют accepted sequence.

Если submit происходит во время обработки предыдущего ingress item, он может только enqueue новый item. Вложенный Lua/native sink call запрещён; внешний pump продолжает FIFO после возврата текущего handler. Session teardown сначала закрывает `Ready`, затем очищает queue и registry.

Текущий vertical slice преобразует UE envelope в STL-only DTO `GV2RuntimeCore::FSemanticInput` и вызывает fixed `game.runtime.dispatch_semantic_input` portable runtime. Lua function name не приходит из Blueprint/binding record и не является generic call API. Внутри Lua Semantic Input преобразуется в `CommandRequest` и сходится с direct headless ingress до Command Dispatcher. Native test observer вызывается только после успешного protected Lua call; Lua error переводит session в `Failed`, блокирует input и очищает registry/ingress.

## Security/authority boundary

- Blueprint не получает authoritative `command_id` и не может подменить binding registry record.
- Blueprint supplies only schema fields exposed by control contract; extra fields и collision с bound args отклоняются.
- Lua revalidates gameplay preconditions; enabled/visible UI state is not authority.
- Raw UObject, asset path, callback name и arbitrary event ID запрещены.

## Input ownership

Physical device state, hover, focus, pressed и repeated-key timing принадлежат UE input layer. Gameplay-significant selection/confirmation becomes semantic input.

Modal input routing is resolved in Presentation before envelope creation. Only the top eligible modal or explicitly permitted layer can submit.

## Result routing

Command Result routes back using correlation ID and resolved current UI identity. A normal refusal can place localized error near `node_key_path`/`element_id` or in configured surface. If UI identity changed before result, Presentation may discard local placement and show only global/system feedback.

## Technical input distinction

Async resource/save/platform completion uses runtime TechnicalInput, not SemanticInput and not gameplay EventBus. It contains operation ID/token-bound DTO and may enqueue a command after validation.

## Tests

Required tests: input-before-ready, unknown/stale handle, stale session/UI/revision, duplicate/out-of-order sequence, removed node, fabricated handle, bound-arg collision, optional/required/extra control fields, malformed values, bounded-capacity rejection, FIFO nested submit without re-entry, modal blocking и late result placement.
