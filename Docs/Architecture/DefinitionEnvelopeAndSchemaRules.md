---
title: Definition Envelope and Schema Rules
status: normative
version: 1.0
updated: 2026-08-10
depends_on:
  - StableIDSpecification.md
decisions:
  - ../ADR/0009-explicit-schema-defaults.md
---

# Definition Envelope and Schema Rules

Документ задаёт единую модель UTF-8 JSON5 definitions и declarative schemas. Runtime, editor, commandlet и CI используют одинаковые правила validation.

## Publication invariant

Candidate repository публикуется только целиком после successful parsing, envelope, schema, normalization, semantic и reference validation. Guessing, partial publish и fallback к предыдущему provider при ошибочном override запрещены.

## Definition file envelope

```json5
{
  schema_version: 1,
  type: "item",
  definitions: [
    {
      id: "core:item.weapon.iron_sword",
      data: {
        label_text_id: "core:text.item.iron_sword.name",
        price: 10,
        icon_resource_id: "core:resource.item.iron_sword.icon",
      },
      tags: ["weapon", "melee"],
      deprecated: false,
      extensions: {},
    },
  ],
  extensions: {},
}
```

Root — закрытый object:

| Поле | Тип | Правило |
|---|---|---|
| `schema_version` | positive int64 | Required; версия schema для `type` |
| `type` | Stable kind token | Required; совпадает с kind каждого `id` |
| `definitions` | array | Required; может быть пустым |
| `extensions` | object | Optional; namespaced blocks |

Один file содержит один type и одну schema version. Package identity берётся из manifest. Duplicate JSON keys и unknown root fields — fatal.

## Definition entry

| Поле | Cardinality | Семантика |
|---|---|---|
| `id` | required | Полный canonical Stable ID |
| `data` | required | Root value, описанный schema |
| `tags` | optional | Уникальные metadata strings; default `[]` для metadata API |
| `deprecated` | optional | Metadata bool; default `false` |
| `extensions` | optional | Namespaced extension blocks |

`data` может быть object, array или scalar, если это разрешает root schema. Entry fields закрыты. `abstract` не входит в общий envelope: если типу нужна такая семантика, она задаётся его `data` schema.

Entry order хранится только как source coordinate. Gameplay order задаётся arrays или explicit ordering field.

## Full override

Одинаковый ID между providers означает полную замену `data`, metadata и extensions. Ничего не наследуется от предыдущего provider.

```json5
// core
{ id: "core:item.potion", data: { price: 10, healing: 5 } }

// mod override
{ id: "core:item.potion", data: { price: 20 } }
```

Override valid только если его собственная запись соответствует schema; отсутствующий required `healing` не берётся из core.

## Schema resources

Schema — package resource со Stable ID kind `schema`. Manifest связывает `(definition_type, schema_version)` с schema resource.

```json5
// manifest fragment
definition_schemas: [
  {
    type: "item",
    schema_version: 1,
    schema_id: "core:schema.definition.item.v1",
    resource: "data/schemas/item_v1.schema.json5",
  },
]
```

```json5
{
  id: "core:schema.definition.item.v1",
  definition_type: "item",
  schema_version: 1,
  root: { kind: "object", fields: {} },
  semantic_validators: ["core:validator.item.semantics"],
  extensions: {},
}
```

- Core предоставляет schemas core kinds.
- Mod, вводящий новый kind, обязан предоставить schema binding до validation его definitions.
- Existing `(kind, version)` schema не override-ится load order-ом.
- Новая семантика schema требует новой version.
- Schema inheritance, mixins и implicit composition отсутствуют.

## Declarative FieldSpec

Common keywords:

| Keyword | Правило |
|---|---|
| `kind` | Required discriminator |
| `required` | Только для object field; default `false` |
| `nullable` | Разрешает explicit JSON null; default `false` |
| `default` | Optional explicit default, проходящий тот же FieldSpec |
| `description` | Tooling metadata |

Supported kinds:

| Kind | Specific keywords |
|---|---|
| `bool` | — |
| `int64` | `min`, `max` |
| `number` | `min`, `max`, `exclusive_min`, `exclusive_max` |
| `string` | `min_length`, `max_length`, `pattern`, `format` |
| `enum` | `values` |
| `array` | `items`, `min_items`, `max_items`, `unique` |
| `map` | `keys`, `values`, `min_entries`, `max_entries` |
| `object` | `fields`, всегда closed |
| `union` | `discriminator`, `variants` |
| `ref` | `target_kind` |
| `text_id` | implicit expected kind `text` |
| `resource_ref` | expected resource class, optional bootstrap requirement |

Physical UE asset path не является значением `resource_ref`. Definition хранит `resource_id`; mapping на Primary Asset ID/Soft Object Path выполняет Presentation/Asset service.

## Presence, defaults и null

| Состояние | Результат |
|---|---|
| Required field absent | `MissingRequiredField` |
| Optional field absent, no default | Поле остаётся отсутствующим |
| Optional field absent, explicit default | Материализуется validated copy default |
| Present null, `nullable: false` | `NullNotAllowed` |
| Present null, `nullable: true` | Сохраняется как `game.null` |

Built-in defaults (`0`, `false`, empty array, first enum/union variant) запрещены. Это сохраняет различие absent/empty и не создаёт скрытую семантику при evolution schema.

## Values и normalization

- Coercion отсутствует: `"10"` не становится number, `0` не становится bool.
- Literal `10` — int64; `10.0` и `1e1` — finite double.
- NaN, infinity и int64 overflow запрещены.
- `-0.0` canonicalizes to `+0.0`.
- String не trim-ится и не case-fold-ится.
- Array order сохраняется; arrays не сортируются и не дедуплицируются.
- Map source order не является gameplay semantics; значимый order хранится array-ем.
- Object fields используют snake_case и closed schema.
- Union выбирается explicit string discriminator без fallback variant.

## Extensions

```json5
extensions: {
  weather_mod: {
    wet_grip_multiplier: 0.85,
  },
}
```

- Key extension block равен package namespace.
- Package пишет только в собственный namespace.
- Extension site/schema регистрируется явно.
- Unknown или foreign extension block — fatal.
- Block валидируется как отдельный closed DTO.
- Full override не наследует extension blocks предыдущей definition.

## Validation pipeline

1. Parse UTF-8 JSON5 и source spans; reject duplicate keys.
2. Validate closed file envelope и entry shell.
3. Resolve `(type, schema_version)` binding.
4. Validate typed structure без coercion.
5. Materialize только explicit defaults.
6. Validate Stable ID namespace ownership и select full-override winners.
7. Run deterministic side-effect-free semantic validators.
8. Resolve `ref`, `text_id` и `resource_ref`.
9. Build immutable candidate and minimal indexes.
10. Publish atomically only when no errors exist.

Semantic validators перечисляются schema в стабильном порядке, читают candidate через read-only interface и не выполняют Lua hooks, I/O, mutation или locale/time-dependent logic.

## Schema evolution

- `schema_version` — positive integer scoped to kind.
- Add/remove required field, change kind/nullability/default/constraints, union/enum semantics или validator set требуют новой version.
- Metadata description можно менять в той же version.
- Migration — explicit editor/bootstrap operation до обычной validation; hidden migration при read запрещена.
- Definition rename выполняется Stable ID redirect, а не schema migration.

## Example schema

```json5
root: {
  kind: "object",
  fields: {
    price: { kind: "int64", required: true, min: 0 },
    label_text_id: { kind: "text_id", required: true },
    icon_resource_id: {
      kind: "resource_ref",
      resource_class: "texture_2d",
    },
    effect: {
      kind: "union",
      required: true,
      discriminator: "kind",
      variants: {
        heal: {
          kind: "object",
          fields: {
            kind: { kind: "enum", required: true, values: ["heal"] },
            amount: { kind: "int64", required: true, min: 1 },
          },
        },
        script: {
          kind: "object",
          fields: {
            kind: { kind: "enum", required: true, values: ["script"] },
            command_id: { kind: "ref", required: true, target_kind: "command" },
          },
        },
      },
    },
  },
}
```

## Conformance

Tests покрывают exact envelope, BOM/comments/trailing comma, duplicate keys/IDs, kind mismatch, scalar roots, unknown fields, explicit defaults, absent/null distinction, numeric types, array order, map order independence, extension ownership, broken override, reference kinds, parallel determinism, schema evolution и no-partial-publication.
