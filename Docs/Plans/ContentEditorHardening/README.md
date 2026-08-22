---
title: Content Editor Hardening and Authoring UX Plan
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - ../Archive/ContentEditor.md
  - ../../Proposals/Archive/ContentEditorPluginProposal.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../../Architecture/GameDataRepositoryContract.md
  - ../../Architecture/BuildAndTooling.md
  - ../../Architecture/DependencyMap.md
decisions:
  - ../../ADR/0037-content-authoring-layer.md
---

# План усиления Content Editor и authoring UX

> **Материализует:** План усиления Content Editor и authoring UX (задачи CEH-01…26).
>
> **Исправляет:** расхождения между принятым Content Editor contract и фактической реализацией.
>
> **Расширяет:** Definition Browser древовидной Stable-ID навигацией и центральную schema-driven форму операциями Add / Remove / Override / Reset / Add Item.
>
> **Задачи:** CEH-01…26.
>
> **Результат:** Content Editor безопасно работает с package overrides, не теряет несохранённые изменения, отличает absent/default/explicit fields, поддерживает структурное редактирование optional properties и arrays, использует typed reference semantics и остаётся frontend-ом единого portable authoring path.

## Цель

Архивный `ContentEditor` подтвердил базовую архитектуру:

```text
JSON5 source of truth
        ↓
GV2ContentAuthoring
        ↓
authoritative candidate validation
        ↓
Editor / CLI frontends
```

Эта архитектура сохраняется.

Новый план не создаёт вторую content model и не превращает Slate UI в источник gameplay/content semantics. Он исправляет обнаруженные дефекты и доводит editor surface до состояния, пригодного для постоянного authoring.

## Состояние на входе

Проверено по текущей реализации.

| Область | Фактическое состояние |
|---|---|
| Browser | Плоский `SListView`; Stable ID hierarchy не используется |
| Definition identity | Editor navigation передаёт только Stable ID; provider/source теряются |
| Package overrides | `IndexedDefinitions` содержит physical entries, но `LoadDefinition(id)` выбирает первый совпавший ID |
| Dirty navigation | `LoadDefinition()` очищает pending edits без navigation guard |
| Stale state | Backend stamp есть, но UI не отслеживает external changes до Save |
| Property presence | UI не отличает absent optional field от explicit zero/false/empty |
| Optional fields | Существующие значения можно менять, но schema-declared absent property нельзя materialize через UI |
| Structural edits | `DirtyFields` выражает только `pointer -> value`; remove/insert/reorder отсутствуют |
| Integer slider | `slider` объединён с number renderer и может менять integer semantic type на double |
| Nested UI metadata | Metadata теряется при рекурсии object fields и для extension fields |
| References | Любая строка, похожая на Stable ID, считается ссылкой; schema semantics не используются |
| Dirty references | Outgoing references читаются из canonical baseline, а не из pending edits |
| Incoming references | Refresh перечитывает и парсит все source files |
| Reference refresh | Может запускаться после каждого символа текстового ввода |
| Resource picker | Повторно читает/parses resource definition files |
| Rename | Переписывает string tokens в owning package, а не schema-typed reference sites |
| Rename impact | External package references явно не моделируются |
| Multi-file rename | Rollback выполняется только при контролируемой write error; crash recovery отсутствует |
| Editor result | Часть multi-file result metadata теряется при adapter conversion |
| UI tests | Основной conformance проверяет service/model, но почти не проверяет реальные Slate round-trips |

## Принятые решения

- Архивный `ContentEditor` не переписывается: это исторический результат CED-01…20.
- Новый план использует task prefix `CEH`.
- Stable ID tree строится только как presentation model; folders/categories в JSON5 не добавляются.
- `:` и `.` используются через canonical `FStableId::Parse()`, а не через отдельный ad-hoc parser.
- Namespace является корневым узлом дерева, kind — следующим уровнем, path segments — последующими уровнями.
- Узел tree может одновременно представлять definition и иметь дочерние path nodes.
- Stable ID недостаточен как physical authoring identity при overrides.
- Для authoring вводится явный locator: package + source + definition ID.
- Browser по умолчанию показывает effective definitions; provider/shadowed information остаётся доступной.
- Arbitrary per-definition properties не вводятся.
- `Add Property` может materialize только поле, разрешённое активной schema/extension schema.
- Absent, implicit schema default, explicit value и explicit null (если schema разрешает null) — разные состояния.
- Required field нельзя удалить через обычный Remove.
- Structural edit остаётся pending до explicit Save; Add/Remove не пишут файл немедленно.
- Presentation hint (`slider`, `multiline`, picker) не меняет semantic value kind.
- References определяются только schema-typed reference sites.
- Runtime `GameDataRepository` не получает обязательный reverse-reference index ради Editor; authoring inspection index является tooling-derived output.
- Rename не должен менять обычную string field только потому, что её текст равен Stable ID.
- External package references не переписываются молча.
- При наличии external references безопасный rename flow должен уметь использовать redirect.
- Expensive derived panels не пересчитываются на каждый keystroke.
- Все mutation operations по-прежнему проходят authoritative candidate validation до commit.

## Границы

Входят:
- portable authoring index/provenance surface;
- provider-aware editor selection;
- Stable-ID tree browser;
- tree search/expand/collapse;
- dirty navigation guard;
- stale file state в UI;
- property presence semantics;
- Add/Remove/Override/Reset optional properties;
- structural array editing;
- nested object editing;
- typed reference inspection;
- rename impact;
- safer multi-file mutation;
- performance hardening;
- реальный Slate interaction conformance.

Не входят:
- отдельный Schema Editor;
- создание новых schema fields из Definition Editor;
- arbitrary raw JSON property insertion;
- raw JSON5 editor;
- bulk multi-definition editor;
- localization translation editor;
- asset import pipeline;
- gameplay/runtime state editor;
- изменение Lua gameplay authority.

## Milestones

- [x] M1 — [Authoring Index and Tree Browser](AuthoringIndexAndTreeBrowser.md): provider-aware identity и Stable-ID tree. CEH-01…05.
- [x] M2 — [Schema-Driven Property Editing](SchemaDrivenPropertyEditing.md): presence model и структурные Add/Remove/Override operations. CEH-06…12.
- [x] M3 — [Typed References and Rename](TypedReferencesAndRename.md): typed reference index, impact и безопасный rename. CEH-13…17.
- [x] M4 — [Session Safety and Performance](SessionSafetyAndPerformance.md): dirty/stale safety и отсутствие expensive refresh на каждый input event. CEH-18…22.
- [x] M5 — [Verification](Verification.md): portable + Slate conformance, scale fixture и финальный gate. CEH-23…26.

## Критический путь

```text
M1 ───────┐
          ├──► M3 ───► M4 ───► M5
M2 ───────┘
```

## Общие правила выполнения

1. Editor не реализует собственную package overlay semantics.
2. Editor не реализует собственную Stable ID grammar.
3. Editor не реализует собственную reference semantics.
4. Slate widgets не выполняют direct filesystem mutation.
5. Любая mutation сначала формирует полный candidate и проходит authoritative validation.
6. Pending edit не обязан сразу существовать в source bytes.
7. `Absent != null != explicit default`.
8. UI renderer не имеет права менять semantic field type.
9. Navigation, destructive operation и tab close проходят через единый dirty-state gate.
10. External modification никогда не перезаписывается молча.
11. Expensive read/index operations имеют измеряемый trigger и не вызываются на каждый символ.
12. Checkbox отмечается только после указанного Evidence.
13. Изменение observable authoring contract синхронно отражается в normative docs и ADR-0037; новый ADR нужен только если меняется архитектурный инвариант.

## Итоговый Definition of Done

- [x] Browser использует `STreeView` и Stable-ID hierarchy.
- [x] Tree строится через canonical Stable ID parser.
- [x] Search сохраняет matching leaves и ancestors; ancestors auto-expand.
- [x] После очистки search пользовательское expansion state восстанавливается.
- [x] Definition node может одновременно иметь definition payload и children.
- [x] Editor selection различает physical provider entries одного Stable ID.
- [x] Effective winner и shadowed providers не вычисляются отдельной editor overlay logic.
- [x] Dirty navigation не может silently discard edits.
- [x] External file modification видна до Save.
- [x] Integer slider round-trip сохраняет integer semantic type.
- [x] Form отличает Absent / ImplicitDefault / Explicit / RequiredMissing.
- [x] `Add Property` предлагает только schema-declared absent optional fields.
- [x] Explicit optional property можно Remove/Reset без immediate disk write.
- [x] Arrays поддерживают Add / Remove / Reorder.
- [x] Nested objects используют ту же presence/edit model.
- [x] Nested и extension UI metadata разрешаются по canonical field path.
- [x] References основаны только на typed schema sites.
- [x] Unsaved reference edit отражается в Outgoing panel до Save.
- [x] Delete blocker использует typed incoming references.
- [x] Resource/reference pickers не перечитывают весь GameData на каждый open/change.
- [x] Rename показывает own-package и external-package impact.
- [x] Rename не переписывает unrelated string fields.
- [x] External references не изменяются молча.
- [x] Multi-file authoring имеет crash-safe recovery contract либо эквивалентный transaction mechanism.
- [x] Text editing не вызывает full reference/index rebuild на каждый символ.
- [x] Init diagnostics и полный multi-file authoring result не теряются в Editor adapter.
- [x] Portable, CLI parity, Slate interaction, override/provider, stale/dirty и performance tests зелёные.
