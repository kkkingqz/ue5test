---
title: UI Documentation Index
status: normative
version: 1.1
updated: 2026-08-10
---

# UI Documentation

UI является перестраиваемой presentation projection. Lua определяет desired composition и gameplay-значимые доступные commands; Unreal реализует widgets, layout, animation, focus, hover и rendering.

## Reading order

1. [UIDocumentAndReconciliation.md](UIDocumentAndReconciliation.md)
2. [SemanticInput.md](SemanticInput.md)
3. [PresentationSnapshotAndEffects.md](PresentationSnapshotAndEffects.md)
4. [WidgetRegistry.md](WidgetRegistry.md)

Общие термины: [../Architecture/GlossaryAndNaming.md](../Architecture/GlossaryAndNaming.md). Command semantics: [../Architecture/CommandsAndEvents.md](../Architecture/CommandsAndEvents.md).

## Core invariants

- Lua не создаёт Widget и не вызывает Blueprint function по имени.
- Blueprint не меняет canonical state и не отправляет gameplay event.
- Physical Widget публикует opaque `binding_handle`; Semantic Input Adapter резолвит его и пересекает Lua boundary только с current bound `command_id`.
- UI-document передаётся целиком; patch protocol отсутствует.
- Reconciliation может переиспользовать physical Widget по stable node key.
- Removed/stale node перестаёт принимать input до завершения exit animation.
- Text использует `text_id`/arguments, assets — `resource_id`.
- Hover, pressed, focus, tooltip и cosmetic animation остаются UE-local.
