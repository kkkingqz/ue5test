---
title: Content Editor Schema-Driven Property Editing Tasks
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - README.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../../ADR/0037-content-authoring-layer.md
---

# M2 — Schema-Driven Property Editing

> **Материализует:** Задачи CEH-06…12.
> **Результат:** центральная форма различает presence state и позволяет materialize/remove schema-declared properties и структурно редактировать arrays/objects без arbitrary JSON properties.

## Задачи

- [ ] **CEH-06 — Разделить semantic field type и presentation hint**
  - Done: semantic kinds остаются semantic kinds; `slider`, `multiline`, picker и другие widget hints выбирают renderer, но не меняют value kind.
  - Integer + slider отображает integer value и пишет `FValue::Integer`, а не `FValue::Number`.
  - Evidence: descriptor tests + real Slate integer/number slider round-trip.

- [ ] **CEH-07 — Ввести Property Presence Model**
  - Done: для каждого schema field форма вычисляет `RequiredMissing`, `Absent`, `ImplicitDefault` или `Explicit`.
  - Explicit null, если разрешён schema, не равен `Absent`.
  - UI больше не показывает технический `0/false/""` как explicit значение отсутствующего поля.
  - Evidence: form-model fixtures.

- [ ] **CEH-08 — Расширить portable authoring edit model структурными operations**
  - Done: pending batch выражает минимум Set/Replace value, Remove object property, Insert array element, Remove array element, Move/Reorder array element.
  - Operations применяются к candidate in-memory до filesystem commit.
  - Batch целиком проходит authoritative repository validation.
  - Любая ошибка оставляет source files неизменными.
  - Existing scalar API может остаться compatibility surface, но Editor использует единый structural batch path.
  - Evidence: portable authoring conformance + negative atomicity tests.

- [ ] **CEH-09 — Реализовать Add / Remove / Override / Reset Property**
  - Зависимости: CEH-07, CEH-08.
  - Done: `Add Property` показывает только отсутствующие optional fields текущей schema/category/object; arbitrary field name ввести нельзя; `ImplicitDefault` показывается как default + `Override`; `Reset to default` и `Remove` удаляют optional property из pending source candidate; required property нельзя удалить обычной командой.
  - Add/Remove не пишет диск до Save.
  - Evidence: Slate interaction + resulting source bytes after Save.

- [ ] **CEH-10 — Nested object editing**
  - Зависимости: CEH-07…09.
  - Done: optional object имеет presence state, может быть materialized через Add и удалён обратно; внутренние properties используют ту же presence model.
  - Object field не превращается в raw JSON textarea.
  - Evidence: nested object schema fixture.

- [ ] **CEH-11 — Array Add / Remove / Reorder**
  - Зависимости: CEH-08.
  - Done: `Add Item` создаёт value по item schema/control; reference array использует typed target picker; array<object> создаёт nested schema-driven editor; Remove удаляет element; Move Up/Down или эквивалентный control меняет order; порядок сохраняется после Save/reload.
  - Negative: item schema/min/max constraints блокируют invalid candidate.
  - Evidence: scalar/reference/object array round-trips.

- [ ] **CEH-12 — Path-based UI metadata для nested и extension fields**
  - Done: label/description/category/order/widget_hint разрешаются по canonical field path, а не теряются при рекурсии.
  - Base schema и package-owned extension schema используют одинаковый metadata-resolution contract.
  - Evidence: nested + extension metadata fixture.

## Проверка milestone

- [ ] Integer slider сохраняет integer.
- [ ] Absent/default/explicit различимы.
- [ ] Add Property не создаёт arbitrary field.
- [ ] Remove/Reset не пишет файл до Save.
- [ ] Nested object использует ту же модель.
- [ ] Array поддерживает Add/Remove/Reorder.
- [ ] Candidate validation остаётся единым write gate.
