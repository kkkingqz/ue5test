---
title: Definition Envelope and Schema Rules
status: normative
version: 2.4
updated: 2026-08-15
depends_on:
  - StableIDSpecification.md
decisions:
  - ../ADR/0009-explicit-schema-defaults.md
  - ../ADR/0018-portable-content-core-module.md
  - ../ADR/0022-external-translation-catalog.md
  - ../ADR/0026-core-and-gameplay-ownership.md
---

# Definition Envelope and Schema Rules

> **Владеет:** конвертом файла определений, устройством схем, типами полей, значениями по умолчанию, расширениями и лимитами парсинга.
> **Не владеет:** разрешением провайдеров и публикацией снимка ([GameDataRepository](GameDataRepositoryContract.md)).
> **Инварианты:** [INV-011](Invariants.md)
> **Реализация:** `Source/GV2ContentCore/Private/Json5Parser.cpp`, `SchemaRegistry.cpp`, `FieldValidation.cpp`, `DefinitionEnvelope.cpp`.
> **Проверки:** `RunJson5ParserConformance`, `RunSchemaRegistryConformance`, `RunScalarValidationConformance`.

Документ задаёт единую модель UTF-8 JSON5 definitions и declarative schemas. Runtime, editor, commandlet и CI используют одинаковые правила validation.

## Publication invariant

Candidate repository публикуется только целиком после successful parsing, envelope, schema, normalization, reference, semantic и repository validation. `FCandidate` обязан явно хранить lifecycle stage: `SchemaValidated` и `ReferencesValidated` являются промежуточными non-publishable artifacts; только полный pipeline со stage `RepositoryResolved` может пересечь publication boundary. Guessing, partial publish и fallback к предыдущему provider при ошибочном override запрещены.

## Accepted JSON5 input

- Input обязан быть bounded valid UTF-8; leading UTF-8 BOM допускается и не входит в parsed value.
- Поддерживаются single-line/block comments, single/double quoted strings, trailing commas, decimal/hex numbers и string line continuation.
- Unquoted key использует portable ASCII grammar `[A-Za-z_$][A-Za-z0-9_$]*`. Любой Unicode key обязан быть quoted; это исключает platform-dependent identifier tables.
- Raw line terminator внутри string, legacy octal escape, malformed Unicode escape и lone UTF-16 surrogate запрещены. Valid surrogate pair декодируется в один Unicode code point.
- Integer хранится как signed int64. Decimal point/exponent создаёт finite double; NaN, infinity и overflow запрещены.
- Parser создаёт transient value tree и source map с JSON Pointer, value span и optional key span. Они существуют только во время candidate build и не входят в immutable repository snapshot/hash.
- Duplicate object key является fatal: diagnostic указывает повторный key основным span и первое объявление related span.
- Пределы парсинга и валидации согласованы с Lua runtime (`FGV2LuaMarshaller`): максимальная глубина вложенности `MaxNestingDepth = 64`, максимальное число элементов в контейнере и узлов в `data` `MaxContainerEntries = 10000`. Превышение лимитов отсекается typed diagnostics на стадии build/parse (`core:diagnostic.json5.limit.nesting_depth`, `core:diagnostic.schema.limit.node_count`), гарантируя беспрепятственное пересечение Lua boundary всеми опубликованными definitions.

## Definition file envelope

```json5
{
  schema_version: 1,
  type: "item",
  definitions: [
    {
      id: "rh:item.weapon.iron_sword",
      data: {
        label_text_id: "rh:text.item.iron_sword.name",
        price: 10,
        icon_resource_id: "rh:resource.item.iron_sword.icon",
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

### Definition envelope validation profile

- `ParseDefinitionFileEnvelope()` является единым portable parser закрытого root и entry shell. Он возвращает immutable `FDefinitionFile`/`FDefinitionEntry` только при полном успехе; partial file запрещён.
- Root допускает только `schema_version`, `type`, `definitions`, `extensions`. Entry допускает только `id`, `data`, `tags`, `deprecated`, `extensions`.
- `schema_version` обязан быть positive int64, `type` — canonical Stable ID segment, `definitions` — array. Каждый entry обязан быть object с canonical `id`, kind которого равен root `type`, и present `data`; explicit null является present data и передаётся root schema без подмены.
- Optional `tags` обязан быть array уникальных strings; metadata defaults равны `[]` и `false` для отсутствующих `tags` и `deprecated`. Optional root/entry `extensions` обязан быть object и по умолчанию равен `{}`. Namespace ownership и extension block content проверяются PCC-19, а не envelope parser.
- `ValidatePackageDefinitionIds()` отклоняет повторный definition ID внутри одного package, включая разные source files, и сохраняет primary/related source spans. Одинаковый ID в разных packages не является duplicate на этой стадии и рассматривается full-override pipeline.
- Envelope diagnostics используют namespaces `core:diagnostic.definition.file.*` и `core:diagnostic.definition.entry.*`; unknown field, invalid shape/type, missing data, malformed ID, ID kind mismatch, invalid metadata и duplicate ID являются fatal и блокируют candidate.

| Failure | Diagnostic code |
|---|---|
| Root не object либо содержит unknown field | `core:diagnostic.definition.file.invalid_shape`, `core:diagnostic.definition.file.unknown_field` |
| Invalid `schema_version`, `type`, `definitions` или root `extensions` | `core:diagnostic.definition.file.invalid_schema_version`, `core:diagnostic.definition.file.invalid_type`, `core:diagnostic.definition.file.invalid_definitions`, `core:diagnostic.definition.file.invalid_extensions` |
| Entry не object либо содержит unknown field | `core:diagnostic.definition.entry.invalid_shape`, `core:diagnostic.definition.entry.unknown_field` |
| Missing/malformed ID либо kind не равен root `type` | `core:diagnostic.definition.entry.invalid_id`, `core:diagnostic.definition.entry.id_kind_mismatch` |
| `data` отсутствует | `core:diagnostic.definition.entry.missing_data` |
| Invalid/duplicate metadata либо entry `extensions` | `core:diagnostic.definition.entry.invalid_tags`, `core:diagnostic.definition.entry.invalid_tag`, `core:diagnostic.definition.entry.duplicate_tag`, `core:diagnostic.definition.entry.invalid_deprecated`, `core:diagnostic.definition.entry.invalid_extensions` |
| Definition ID повторён внутри package | `core:diagnostic.definition.entry.duplicate_id` |

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
    schema_id: "rh:schema.definition.item.v1",
    resource: "schemas/item_v1.schema.json5",
  },
]
```

```json5
{
  id: "rh:schema.definition.item.v1",
  definition_type: "item",
  schema_version: 1,
  root: { kind: "object", fields: {} },
  semantic_validators: [],
  extensions: {},
}
```

- Core предоставляет schemas только для framework kinds, необходимых runtime или host boundary (`screen`, `text`, `resource`, минимальный `actor`).
- Gameplay package или mod объявляет schema binding для kind (например, `item`, `location`), если ядро само не объявляет binding для этой пары `(definition_type, schema_version)` (ADR-0026).
- Existing `(kind, version)` schema не override-ится load order-ом: конфликт двух bindings для одной пары `(definition_type, schema_version)` остаётся fatal.
- Новая семантика schema требует новой version.
- Schema inheritance, mixins и implicit composition отсутствуют.

Schema resource envelope является closed object:

| Поле | Тип | Правило |
|---|---|---|
| `id` | schema Stable ID | Required; обязан точно совпасть с descriptor binding |
| `definition_type` | canonical segment | Required; не выводится из filename/path |
| `schema_version` | positive int64 | Required; часть exact registry key |
| `root` | object FieldSpec | Required; компилируется последующими validation stages |
| `semantic_validators` | array of validator Stable IDs | Optional; default `[]`, duplicates запрещены |
| `extensions` | object | Optional; default `{}`; ownership проверяется extension stage |

`FSchemaRegistry` регистрирует только exact key `(definition_type, schema_version)`. Version fallback запрещён. Descriptor binding содержит все четыре значения: `definition_type`, `schema_version`, `schema_id`, package-relative resource path. Resource identity обязана совпасть с первыми тремя, а прочитанный source — с path. Duplicate exact key, conflicting schema ID, повторное использование одного `schema_id` для другого key и definition source без exact binding являются fatal. Load order не разрешает эти конфликты.

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
| `resource_ref` | `resource_class`, optional `bootstrap_required` |

### Scalar validation profile

- `bool`, `int64`, `number`, `string` и `enum` компилируются в portable `FScalarFieldSpec`; validation present value выполняется одним `ValidateScalarValue()` path.
- `int64` принимает только integer value и inclusive `min`/`max` int64. `number` принимает только double value; integer не coercing в double. Его `min`/`max` inclusive, `exclusive_min`/`exclusive_max` задают numeric exclusive bound; inclusive и exclusive bound одной стороны одновременно запрещены.
- String `min_length`/`max_length` считают Unicode code points корректного UTF-8, а не bytes. `pattern` является full-match ECMAScript regular expression. `format` v1 принимает только `stable_id` и `stable_id_segment` и использует canonical `FStableId`, не отдельную grammar.
- `enum.values` является непустым ordered array уникальных non-null scalar values. Сравнение exact: integer `2` и number `2.0` различаются.
- Explicit null разрешён только при `nullable: true`; отсутствие значения проверяется отдельно и не представляется null.
- Scalar FieldSpec является closed для своего kind. Unknown keyword, неверный тип constraint, invalid range/pattern/format и duplicate enum value являются schema errors до validation definitions.
- Value failure использует `core:diagnostic.schema.value.type_mismatch`, `core:diagnostic.schema.value.null_not_allowed`, `core:diagnostic.schema.value.enum_not_allowed` либо `core:diagnostic.schema.value.constraint_failed` и обязан содержать JSON Pointer, value source span и доступные package/definition/schema identities.

### Container validation profile

- Все scalar и container nodes компилируются рекурсивно одним `CompileFieldSpec()` в immutable `FCompiledFieldSpec`. `ValidateFieldValue()` является единственным recursive validation path; schema registry не хранит отдельные scalar/container adapters.
- `array.items` обязателен. `min_items`/`max_items` являются non-negative int64, `unique` — boolean с default `false`. Validation не изменяет, не сортирует и не дедуплицирует array; uniqueness использует exact portable value comparison без копирования элементов. Array order является значимым, object/map keys сравниваются canonical order-independently.
- `map` представлен JSON object. `keys` и `values` обязательны; key schema допускает только `string` либо string-only `enum`, а `min_entries`/`max_entries` являются non-negative int64. Key failure указывает JSON Pointer entry и key source span; value failure — тот же pointer и value span.
- `object.fields` является обязательным object, его имена используют canonical `snake_case`. Object всегда closed: любое present поле вне `fields` отклоняется с `core:diagnostic.schema.value.unknown_field`.
- `union.discriminator` является canonical `snake_case` field name, `variants` — непустой object explicit string tag → object FieldSpec. Missing/non-string discriminator даёт `core:diagnostic.schema.value.invalid_union_discriminator`, неизвестный tag — `core:diagnostic.schema.value.invalid_union_variant`; fallback variant запрещён.
- Container type mismatch, size constraint и duplicate array item используют соответственно `core:diagnostic.schema.value.type_mismatch`, `core:diagnostic.schema.value.constraint_failed` и `core:diagnostic.schema.value.duplicate_array_item`. Diagnostics обязаны сохранять доступные package/definition/schema identities, JSON Pointer и source span.

### Special field validation profile

- `ref`, `text_id` и `resource_ref` являются полноценными leaf nodes общего compiled `FCompiledFieldSpec`; deferred/host-specific validator path запрещён.
- Present special value обязан быть string с canonical Stable ID по единому `GV2ContentCore::FStableId`. `GV2RuntimeCore::FStableId` является alias того же utility; валидация schema не содержит отдельной grammar.
- `ref.target_kind` обязателен и является canonical Stable ID segment. Значение `ref` обязано иметь этот kind.
- `text_id` не имеет specific keywords и неявно требует Stable ID kind `text`.
- `resource_ref.resource_class` обязателен и является canonical segment. `bootstrap_required` optional boolean с default `false`; metadata сохраняется в compiled schema для последующего resolution. Значение `resource_ref` обязано иметь Stable ID kind `resource`.
- Каждый special FieldSpec closed. Неверные `target_kind`, `resource_class` и `bootstrap_required` отклоняются при schema compilation; explicit default проходит тот же special validator.
- Non-string value использует `core:diagnostic.schema.value.type_mismatch`, malformed Stable ID — `core:diagnostic.schema.value.invalid_stable_id`, valid ID неправильного kind — `core:diagnostic.schema.value.stable_id_wrong_kind`. Diagnostic сохраняет expected kind в message, JSON Pointer, source span и доступную provenance context.
- Shape validation PCC-17 не проверяет существование target и соответствие resolved resource definition объявленному `resource_class`. PCC-24 выполняет эти проверки одним recursive path для `data` и materialized `definition_entry` extension blocks только после full-override winner selection; canonical ID отсутствующей definition допустим до этой стадии, но блокирует `ReferencesValidated` artifact.

Physical UE asset path не является значением `resource_ref`. Definition хранит `resource_id`; mapping на Primary Asset ID/Soft Object Path выполняет Presentation/Asset service.

## Presence, defaults и null

| Состояние | Результат |
|---|---|
| Required field absent | `core:diagnostic.schema.value.missing_required_field` |
| Optional field absent, no default | Поле остаётся отсутствующим |
| Optional field absent, explicit default | Материализуется validated copy default |
| Present null, `nullable: false` | `core:diagnostic.schema.value.null_not_allowed` |
| Present null, `nullable: true` | Сохраняется как `game.null` |

Built-in defaults (`0`, `false`, empty array, first enum/union variant) запрещены. Это сохраняет различие absent/empty и не создаёт скрытую семантику при evolution schema.

- Explicit `default` компилируется и рекурсивно проверяется тем же `FCompiledFieldSpec`, что runtime value. Invalid default является schema error и указывает JSON Pointer самого `default` либо вложенного значения.
- `required: true` вместе с `default` запрещён как противоречивый FieldSpec: required absence всегда остаётся ошибкой.
- Default применяется только к отсутствующему optional object field. Present null, empty string/object/array и scalar zero не заменяются default.
- `ValidateFieldValue()` строит отдельный materialized `FValue` и присваивает output только после полного успеха. Input, compiled default и ранее выданный output при ошибке не мутируются.
- `ValidateExtensionBlocks()` применяет тот же transactional rule ко всему namespaced extensions object и возвращает отдельный materialized output. Definition-entry blocks, включаемые в M3 stage artifact, берутся только из этого output; explicit defaults extension schema не могут быть отброшены.
- Каждый materialized default является глубоким value-copy. Изменение array/object одной resolved definition не может изменить compiled schema либо другую materialization.
- Present object fields сохраняют source order; отсутствующие defaulted fields добавляются в declaration order schema. Последующая canonical snapshot normalization не использует object source order как gameplay semantics.

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

- Key extension block равен package namespace. Package пишет только в собственный namespace; definition, fully overriding foreign ID, всё равно использует namespace своего package.
- Extension schema регистрируется отдельным exact descriptor binding с key `(definition_type, schema_version, extension_site, extension_namespace)`. Version/site fallback и implicit schema discovery запрещены.
- `extension_site` принимает только `definition_file`, `definition_entry`, `schema_resource`. Они соответствуют `extensions` root definition file, definition entry и основной schema resource. Extension schema resource сам не имеет `extensions`, поэтому recursive extension registration отсутствует.
- Binding и resource принадлежат `extension_namespace`; она обязана равняться immutable namespace/package ID provider-а. `schema_id` обязан быть package-owned canonical ID kind `schema`.
- Exact основная schema `(definition_type, schema_version)` обязана существовать до регистрации extension schema. Extension binding не заменяет и не изменяет основную schema.
- Unknown, unregistered или foreign extension block — fatal. Block проверяется общим `CompileFieldSpec()`/`ValidateFieldValue()` как отдельный closed object DTO; root extension FieldSpec другого kind запрещён.
- Full override хранит собственные `data`, metadata и `extensions` как одну entry. Extension blocks shadowed provider-а не наследуются и не merge-ятся.

```json5
// manifest fragment of weather_mod
extension_schemas: [
  {
    type: "item",
    schema_version: 1,
    extension_site: "definition_entry",
    extension_namespace: "weather_mod",
    schema_id: "weather_mod:schema.extension.item.entry.v1",
    resource: "schemas/item_weather_entry_v1.schema.json5",
  },
]
```

```json5
// schemas/item_weather_entry_v1.schema.json5
{
  id: "weather_mod:schema.extension.item.entry.v1",
  definition_type: "item",
  schema_version: 1,
  extension_site: "definition_entry",
  extension_namespace: "weather_mod",
  root: {
    kind: "object",
    fields: {
      wet_grip_multiplier: { kind: "number", required: true, min: 0.0, max: 1.0 },
    },
  },
}
```

Extension schema resource является closed object с полями `id`, `definition_type`, `schema_version`, `extension_site`, `extension_namespace`, `root`. Descriptor/resource identity обязана совпадать полностью. Duplicate exact binding, reuse одного `schema_id`, missing target schema и mismatch являются fatal.

| Failure | Diagnostic code |
|---|---|
| Invalid descriptor binding | `core:diagnostic.repository.package_set.invalid_extension_schema_binding` |
| Duplicate descriptor exact key | `core:diagnostic.repository.package_set.duplicate_extension_schema_binding` |
| Invalid/unknown extension schema resource | `core:diagnostic.extension.schema.invalid_*`, `core:diagnostic.extension.schema.unknown_field` |
| Descriptor/resource mismatch или missing target | `core:diagnostic.extension.schema.resource_mismatch`, `core:diagnostic.extension.schema.target_schema_missing` |
| Duplicate exact registration/schema ID | `core:diagnostic.extension.schema.duplicate_binding`, `core:diagnostic.extension.schema.schema_id_conflict` |
| Invalid, foreign или unregistered block namespace/site | `core:diagnostic.extension.block.invalid_namespace`, `core:diagnostic.extension.block.foreign_namespace`, `core:diagnostic.extension.block.unregistered_site` |
| Block не соответствует registered schema | Общий `core:diagnostic.schema.value.*` с `SchemaId` extension schema |

## Validation pipeline

1. Parse UTF-8 JSON5 и source spans; reject duplicate keys.
2. Validate closed file envelope и entry shell.
3. Resolve `(type, schema_version)` binding.
4. Validate typed structure без coercion.
5. Materialize только explicit defaults, включая registered extension blocks. После этой стадии M3 может вернуть только явно помеченный non-publishable `SchemaValidated` artifact.
6. Validate Stable ID namespace ownership и select full-override winners.
7. Validate и flatten redirects/tombstones; reject conflicts/chains без final active target.
8. Resolve `ref`, `text_id` и `resource_ref` против final winner set и materialize canonical targets.
9. Run deterministic side-effect-free semantic validators.
10. Build provenance/minimal indexes/hash.
11. Build immutable `RepositoryResolved` candidate.
12. Publish atomically only when no errors exist.

Semantic validators перечисляются schema в стабильном порядке. `ISemanticValidator::Validate()` получает current materialized winner, `FSemanticCandidateView`, immutable provenance context и diagnostic sink. Candidate view предоставляет только read operations над winners; validator не может заменить definition или изменить candidate. Core и optional host registry ищутся по exact validator Stable ID. Unavailable validator, invalid/duplicate registration и любой validator diagnostic являются fatal. Lua hooks, I/O, mutation, locale/time-dependent logic и wall-clock dependency запрещены.

## Schema evolution

- `schema_version` — positive integer scoped to kind.
- Add/remove required field, change kind/nullability/default/constraints, union/enum semantics или validator set требуют новой version.
- Metadata description можно менять в той же version.
- Migration — explicit editor/bootstrap operation до обычной validation; hidden migration при read запрещена.
- Definition rename выполняется Stable ID redirect, а не schema migration.

## Example schema

Этот минимальный `item` schema является executable fixture M3 и соответствует `Tests/Fixtures/PortableContentCore/valid/core/schemas/item_v1.schema.json5`.

```json5
root: {
  kind: "object",
  fields: {
    price: { kind: "int64", required: true, min: 0 },
    label_text_id: { kind: "text_id", required: true },
    icon_resource_id: {
      kind: "resource_ref",
      required: true,
      resource_class: "texture_2d",
    },
  },
}
```

### Minimal core schema set

M3/M4 фиксируют следующие definition kinds:

| Kind | Минимальные поля `data` |
|---|---|
| `location` | required `title_text_id`; required non-empty unique `screen_ids` refs kind `screen` |
| `screen` | required `title_text_id` |
| `item` | required `label_text_id`, non-negative `price`, required `icon_resource_id` class `texture_2d` |
| `text` | required non-empty `source_message` (`min_length: 1`) |
| `resource` | required `resource_class` enum `texture_2d`; required `required` bool |
| `actor` | required `archetype` (`min_length: 1`) |

- **Семантика `source_message` в схеме `text`** ([ADR-0022](../ADR/0022-external-translation-catalog.md)):
  - Поле `source_message` — обязательная непустая строка (`min_length: 1`) на языке авторинга.
  - Служит контекстом для переводчиков при извлечении в PO-каталоги (`<package-root>/localization/<locale>.po`) и fallback-строкой при отсутствии перевода в целевой локали (а также в headless-хосте, где переводы не загружаются).
  - `source_message` **не является** «переводом на язык по умолчанию» и не заменяет PO-каталог локализации.
  - Gameplay-логика и Lua-скрипты никогда не читают и не сравнивают `source_message` как условие: игровой код оперирует исключительно Stable ID (`text_id`), а текст форматируется и локализуется строго на presentation-границе.
  - Пустая строка `""` или отсутствие поля отклоняются валидацией схемы с ошибкой (`core:diagnostic.schema.value.constraint_failed` или `core:diagnostic.schema.value.missing_required_field`).

Canonical schema resources и representative definitions находятся в `Tests/Fixtures/PortableContentCore/valid/core`. `GV2ContentCore::Testing::MakeRepresentativeCorePackageDescriptor()` является общей UE/headless привязкой этих fixture sources. Representative package содержит locations, screens, items, texts, resources и actors. `quest`, trigger/effect DSL и per-kind native managers отсутствуют.

## Conformance

Реализованный shared UE/headless conformance покрывает полный M3/M4 path: package-local ownership, full/broken override, redirect/tombstone resolution, canonical reference rewrite, semantic validation, provenance, minimal indexes, deterministic hash и immutable read handle. Host publication/restart semantics и Lua read adapters относятся к следующим milestones.
