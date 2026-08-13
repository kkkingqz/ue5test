---
title: CommonUI Runtime Integration Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-08-13
depends_on:
  - ../Architecture/SystemContextAndComponents.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../UI/ScreenTemplates.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../UI/SemanticInput.md
decisions:
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
---

# Предложение по интеграции CommonUI runtime

## Назначение и область

CommonUI уже включён в проект и должен стать UE-side механизмом physical input routing, focus restoration, activatable lifecycle и platform-aware UI navigation. Lua UI-document остаётся единственным источником desired Screen instances и Command bindings.

## Разделение ответственности

| Ответственность | Owner |
|---|---|
| Desired route, overlays, modals и gameplay availability | Lua UI projection |
| Screen identity и full reconciliation | GV2 Presentation reconciler |
| Physical device routing, focus, input mode и activation lifecycle | CommonUI / UE Presentation |
| Gameplay-significant Back/Confirm | Opaque binding → Semantic Input → Command Dispatcher |
| Popup, hover, scroll и cosmetic local close | UE-local Widget state |

CommonUI container не может самостоятельно сделать permanent pop Screen instance, который остаётся в current desired document. Изменение desired route сначала проходит Lua Command path, затем reconciler применяет новую revision.

## Предлагаемая структура

- `UGV2ScreenWidgetBase` переходит с `UCommonUserWidget` на `UCommonActivatableWidget`.
- `WBP_ScreenBase` остаётся abstract Blueprint base, а concrete Screen Templates наследуют его без native per-screen classes.
- `WBP_GameShell` хранит named CommonUI-compatible layer hosts.
- Exclusive main route и top modal используют activatable container semantics.
- Одновременно видимые overlays не принуждаются к stack semantics; их host выбирается по зарегистрированной layer policy.
- Reconciler остаётся owner create/reuse/remove и atomic binding publication.

Точный container class для каждого layer выбирается одним небольшим integration spike и фиксируется в UI contract. Универсальная собственная navigation framework поверх CommonUI не создаётся.

## Activation и reconciliation

1. Reconciler валидирует полный candidate UI-document и bindings.
2. Screen Registry разрешает trusted `UCommonActivatableWidget` subclass.
3. Candidate Screen Fields применяются до input activation.
4. Reconciler обновляет layer hosts и активирует только logically interactive instances.
5. После successful activation/focus preparation binding revision становится current.
6. Removed instance сначала теряет handles/input, затем может проиграть exit transition.

CommonUI activation hooks не вызывают Lua function и не меняют canonical gameplay-state. Они могут управлять focus, input mapping context, animation и UE-local cleanup.

## Back semantics

- Back, закрывающий только transient local popup, обрабатывается внутри UE.
- Back, который меняет route/modal/overlay desired document, обязан использовать validated opaque binding.
- Widget не хранит `command_id`; binding создаётся reconciler-ом из Lua document.
- Если current interactive Screen не объявляет Back binding, CommonUI не выполняет hidden gameplay navigation.
- Добавление optional Screen Instance `back_binding` меняет UI-document schema и до реализации требует ADR/обновления `UIDocumentAndReconciliation.md` и `SemanticInput.md`.

На первом этапе можно оставить gameplay Back как явную Screen Field кнопку. Это позволяет внедрить focus/activation без одновременного изменения wire schema.

## Этапы внедрения

1. Перевести base Screen на `UCommonActivatableWidget`; обновить Blueprint assets и lifecycle tests.
2. Подключить Game Shell layer hosts и focus restoration для route/modal.
3. Централизовать physical Back/Confirm routing без нового gameplay ingress.
4. Добавить schema-defined `back_binding` отдельным ADR только при concrete screen need.
5. Добавить platform/input-method visual prompts, если появится UX consumer.

## Польза, риски и трудоёмкость

- **Польза:** готовая focus/navigation infrastructure, единое поведение keyboard/gamepad и меньше custom UI lifecycle code.
- **Трудоёмкость:** **M**.
- **Риск:** два navigation owners. Мера — CommonUI реализует physical lifecycle, Lua document определяет desired membership.
- **Риск:** activation до atomic field/binding commit. Мера — input закрыт до полного candidate apply.
- **Риск:** неподходящие stack semantics для overlays. Мера — layer policy выбирает container behavior, без universal stack.
- **Риск:** migration Blueprint assets. Мера — staged reparent, compile/save и asset audit через Unreal Editor API.

## Не входит в предложение

- Новый gameplay navigation service в C++.
- Command IDs, Lua callbacks или raw UObject в Widget bindings.
- Lua-authored physical Widget tree.
- Собственная замена CommonUI focus/input routing.

## Критерии приёмки

- Concrete Screen Templates являются activatable и продолжают применять тот же generic Screen Field contract.
- Route/modal focus корректно восстанавливается для keyboard и gamepad после reconciliation.
- Removed/stale Screen теряет input до exit transition.
- UE-local popup Back не входит в Lua; desired route/modal Back проходит общий Semantic Input emitter.
- CommonUI code не появляется в portable modules и headless build.
- Tests подтверждают отсутствие direct CommonUI pop как замены Lua document update.
