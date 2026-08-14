---
title: Portable Content Core Schema Validation Tasks
status: draft
version: 1.0
updated: 2026-08-13
depends_on:
  - Json5Parsing.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../../Architecture/StableIDSpecification.md
---

# M3 — Definition Envelope и Schema Validation

## Результат этапа

Один core package с `location`, двумя `screen` и `item` definitions проходит полную envelope/schema validation. Invalid package возвращает точные diagnostics и не создаёт candidate.

## Задачи

- [x] **PCC-13 — Реализовать schema resource parser и registry**
  - Зависимости: PCC-09, PCC-10.
  - Регистрировать exact binding `(definition_type, schema_version) → schema_id` из package descriptor.
  - Done: missing, duplicate и conflicting bindings отклоняются; kind не выводится из filename.
  - Evidence: `GV2ContentCore::ParseSchemaResource`, `GV2ContentCore::FSchemaRegistry`, integration через `BuildRepository()`; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.SchemaRegistry` и `GV2.Runtime.ContentCore.PackageDescriptorValidation`.

- [x] **PCC-14 — Реализовать scalar validation**
  - Зависимости: PCC-13.
  - Поддержать `bool`, `int64`, `number`, `string`, `enum`, nullable и объявленные constraints без coercion.
  - Done: type/constraint failures имеют stable code, JSON pointer и source span.
  - Evidence: `GV2ContentCore::CompileScalarFieldSpec`, `GV2ContentCore::ValidateScalarValue`, scalar node compilation через recursive `CompileFieldSpec()` в `ParseSchemaResource()` и present `data` validation через `BuildRepository()`; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.ScalarValidation`.

- [x] **PCC-15 — Реализовать container validation**
  - Зависимости: PCC-14.
  - Поддержать `array`, `map`, closed `object` и explicit-discriminator `union`.
  - Done: unknown object fields и invalid union variant отклоняются; array order сохраняется.
  - Evidence: recursive `GV2ContentCore::FCompiledFieldSpec`, `CompileFieldSpec()` и `ValidateFieldValue()`; exact schema compilation в `ParseSchemaResource()` и recursive present `data` validation через `BuildRepository()`; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.ContainerValidation`.

- [x] **PCC-16 — Реализовать presence, null и explicit defaults**
  - Зависимости: PCC-14, PCC-15.
  - Сохранить различие absent/null/empty; материализовать только declared validated default.
  - Done: built-in defaults и hidden coercion отсутствуют; default копируется, а не разделяет mutable storage.
  - Evidence: `FCompiledFieldSpec::DefaultValue`, transactional `ValidateFieldValue(..., OutMaterializedValue, ...)` и `ValidateExtensionBlocks(..., OutMaterializedExtensions, ...)`; schema-time validation explicit defaults и recursive materialization object/extension fields; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.PresenceAndDefaults` и `GV2.Runtime.ContentCore.ExtensionSchema`.

- [x] **PCC-17 — Реализовать special field kinds**
  - Зависимости: PCC-14, canonical `GV2RuntimeCore::FStableId`.
  - Поддержать `ref`, `text_id`, `resource_ref` и expected kind/resource class metadata.
  - Done: validation grammar использует общий Stable ID parser; фактическое target resolution отложено до PCC-24.
  - Evidence: compiled `EFieldKind::Reference`, `TextId`, `ResourceReference` metadata и единый `ValidateFieldValue()` path через canonical `GV2ContentCore::FStableId`; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.SpecialFieldValidation`.

- [x] **PCC-18 — Реализовать Definition file envelope**
  - Зависимости: PCC-13–PCC-17.
  - Проверить closed root/entry, `schema_version`, `type`, ID kind, `data`, tags, deprecated и extensions shell.
  - Done: duplicate definition внутри package, unknown fields и kind mismatch дают fatal diagnostics.
  - Evidence: portable `FDefinitionFile`/`FDefinitionEntry`, `ParseDefinitionFileEnvelope()` и `ValidatePackageDefinitionIds()` в единственном `BuildRepository()` path; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.DefinitionEnvelope`.

- [x] **PCC-19 — Реализовать extension block validation**
  - Зависимости: PCC-13, PCC-18.
  - Проверить package namespace ownership и explicit extension schema/site registration.
  - Done: unknown/foreign block отклоняется; extension не наследуется через будущий full override.
  - Evidence: `FExtensionSchemaBinding`, portable `FExtensionSchemaRegistry`, `ParseExtensionSchemaResource()` и `ValidateExtensionBlocks()` для exact sites `definition_file`/`definition_entry`/`schema_resource` в `BuildRepository()`; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.ExtensionSchema` и `GV2.Runtime.ContentCore.PackageDescriptorValidation`.

- [x] **PCC-20 — Добавить минимальные core schemas**
  - Зависимости: PCC-13–PCC-19.
  - Реализовать только поля vertical slice для `location`, `item`, `screen` и необходимые identities/metadata `text` и `resource`.
  - Done: documentation fixtures и один representative core package проходят validation; лишние будущие kinds отсутствуют.
  - Evidence: shared fixtures `valid/core/schemas/{location,screen,item,text,resource}_v1.schema.json5`, `GV2ContentCore::Testing::MakeRepresentativeCorePackageDescriptor()`, historical M3 `SchemaValidated` lifecycle stage; после завершения M4 тот же reference path возвращает publishable `RepositoryResolved`; CTest `gv2_headless_self_test`; Unreal automation `GV2.Runtime.ContentCore.MinimalCoreSchemas`.

## Проверка milestone

- [x] Valid core package полностью проходит через library `BuildRepository()`.
- [x] Invalid envelope/schema/default/reference-shape cases возвращают ordered diagnostics.
- [x] Пример из `DefinitionEnvelopeAndSchemaRules.md` адаптирован к canonical minimal item fixture и проверяется общим UE/headless corpus.
- [x] `actor`, `quest`, trigger/effect DSL и per-kind native managers не добавлены.
