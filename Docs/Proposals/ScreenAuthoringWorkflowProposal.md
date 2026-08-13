---
title: Screen Authoring Workflow Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-08-13
depends_on:
  - CommonUIRuntimeIntegrationProposal.md
  - ../UI/ScreenTemplates.md
  - ../UI/WidgetRegistry.md
  - ../UI/UIDocumentAndReconciliation.md
decisions:
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
---

# Предложение по authoring workflow Screen Templates

## Назначение и область

UMG Designer остаётся canonical visual editor для Screen Templates. Предлагается небольшой Editor-only tool, который создаёт правильную заготовку, регистрирует Screen и проверяет contract. Собственный drag-and-drop canvas и bidirectional Widget Tree serialization не создаются.

## Источник истины

- Widget Blueprint владеет layout, animation, focus graph и composition.
- Screen Registry владеет mapping `screen_id → trusted Screen Template class + layer policy`.
- Dynamic Screen Elements в Blueprint объявляют `field_id`, `schema_id` и required policy.
- Lua владеет только full desired Screen Fields и bindings.
- Generated reports являются diagnostics artifacts и не импортируются обратно как layout source.

## Editor-only граница

Tool предлагается оформить как project plugin `GV2EditorTools` с одним Editor module. Runtime module, packaged build, `GV2RuntimeCore` и `GV2ContentCore` не зависят от него.

Минимальные команды:

- **Create GV2 Screen** — создаёт `WBP_Screen_<Name>` как child `WBP_ScreenBase`.
- **Register Screen** — добавляет canonical `screen_id`, class и layer policy в Screen Registry через editor transaction.
- **Validate Selected Screen** — проверяет один Blueprint и его registry entry.
- **Validate All GV2 Screens** — запускает тот же validator для Editor/CI commandlet.

AI создаёт и изменяет `.uasset` только через Unreal Editor API, затем compile, save и validation. Tool не пишет бинарные assets напрямую.

## Validation rules

Validator обязан проверять:

1. Concrete class является non-abstract child `WBP_ScreenBase`.
2. `screen_id` canonical и unique; registry class/layer valid.
3. Dynamic Screen Elements имеют unique lowercase `field_id`.
4. Каждый `schema_id` canonical, зарегистрирован и совместим с adapter type.
5. Required/optional declarations не конфликтуют.
6. Required `BindWidget` существует и имеет ожидаемый class.
7. Runtime text/content images/repeated elements/Semantic Input используют approved leaf adapters и emitter.
8. Scrollable element получает bounded geometry от Screen layout.
9. CommonUI activation/focus requirements выполнены после его внедрения.
10. Blueprint compiles без errors и все referenced assets cookable.

Validation выдаёт stable diagnostics с asset, `screen_id`, `field_id`, `schema_id` и source object path только как UE-local authoring provenance. Raw asset locator не попадает в Lua/content DTO.

## Generated contract summary

Tool может создавать read-only machine report в `Saved/GV2/ScreenContracts/`:

```json5
{
  screen_id: "core:screen.main",
  layer: "location_content",
  fields: [
    {
      field_id: "buttons",
      schema_id: "core:schema.ui_field.button_list.v2",
      required: true,
    },
  ],
}
```

Report используется CI, diff/debug и AI inspection. Он не хранится как второй normative source, не импортируется в Widget Blueprint и не содержит physical Widget tree.

## Authoring flow

1. Content author запускает **Create GV2 Screen**.
2. Редактирует layout стандартным UMG Designer и размещает existing Dynamic Screen Elements.
3. Назначает `field_id`/`schema_id` в Details panel.
4. Регистрирует `screen_id` и layer policy.
5. Tool компилирует Blueprint и запускает contract validation.
6. Lua producer добавляет full Screen Fields и проходит UE/headless contract fixture.
7. CI повторяет asset audit и runtime integration test.

## Польза, риски и трудоёмкость

- **Польза:** дизайнер сохраняет стандартный UMG workflow, а schema/registry ошибки обнаруживаются до runtime.
- **Трудоёмкость:** **M**.
- **Риск:** tool превращается в второй UI editor. Мера — только creation, registry operations, validation и reports.
- **Риск:** Widget tree export становится source of truth. Мера — report read-only, one-way и хранится в `Saved`.
- **Риск:** validator дублирует runtime logic. Мера — общие descriptor/query helpers; runtime остаётся окончательной проверкой candidate apply.
- **Риск:** editor module попадает в packaged build. Мера — Editor-only plugin/module и build dependency audit.

## Не входит в предложение

- Полный export/import Widget Tree в JSON5.
- Собственный layout format, designer canvas или Blueprint replacement.
- Автогенерация per-screen C++ classes.
- Lua selection физического Widget class/path.
- Автоматическое создание нового field schema без concrete runtime adapter.

## Критерии приёмки

- Новый Screen создаётся, регистрируется и проходит validation без изменения runtime C++ для concrete `screen_id`.
- Duplicate/invalid IDs, schema mismatch, missing `BindWidget`, direct parallel presentation path и unbounded scrollable layout обнаруживаются до play.
- Editor и CI возвращают одинаковые stable diagnostic codes.
- Generated summary не требуется для runtime и может быть полностью удалён без изменения behavior.
- Plugin отсутствует в packaged runtime dependency graph.
