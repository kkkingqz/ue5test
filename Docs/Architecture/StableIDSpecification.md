---
title: Stable ID Specification
status: normative
version: 1.5
updated: 2026-08-15
decisions:
  - ../ADR/0002-stable-id-format.md
  - ../ADR/0023-stable-id-publication-freeze.md
---

# Stable ID Specification

> **Владеет:** грамматикой идентификаторов, реестром kind, владением namespace, redirects, локальными и persistent instance ID, жизненным циклом публикации.
> **Не владеет:** тем, какие сущности существуют, — это контент и его схемы.
> **Инварианты:** [INV-009](Invariants.md), [INV-015](Invariants.md)
> **Реализация:** `Source/GV2ContentCore/Private/StableId.cpp`, `Scripts/runtime/stable_id.lua`.
> **Проверки:** `RunStableIdConformance`.

Stable ID описывает логическую идентичность и не зависит от filename, directory, display text, locale, provider order, UE object path или memory address.

## Global grammar

```text
stable-id    = namespace ":" kind "." path
namespace    = segment
kind         = segment
path         = segment *("." segment)
segment      = lowercase-alpha *(lowercase-alpha / digit / "_")
```

Ограничения:

- segment: 1–64 ASCII characters;
- полный ID: не более 192 ASCII characters;
- только `a-z`, `0-9`, `_`, `.`, `:`;
- segment начинается с `a-z`;
- ровно одно `:` между namespace и kind;
- пустые segments, whitespace, Unicode, hyphen, trailing dot и repeated dots запрещены.

Примеры:

```text
rh:item.weapon.iron_sword
rh:location.city.market
core:command.location.travel
core:event.location.enter
weather_mod:item.ring.storm
```

## Canonical input policy

Runtime parser ничего не исправляет: не trim-ит whitespace, не lowercases, не заменяет punctuation и не выполняет Unicode normalization. Невалидный input отклоняется.

Editor/CLI может предложить отдельную fix-команду, но результат повторно проходит strict parser. Registry, save и logs хранят только canonical bytes.

## Runtime parser API

`GV2ContentCore::FStableId` является единственной C++-реализацией global grammar и `segment`. Utility находится в нижнем portable Content module, принимает UTF-8 `std::string_view`, не изменяет input и не зависит от Unreal Engine, Lua или filesystem types. `GV2RuntimeCore::FStableId` является только compatibility alias к этому типу и не содержит второй grammar.

- `Parse` валидирует global Stable ID и возвращает views `Namespace`, `Kind`, `Path`.
- `IsOfKind` выполняет тот же полный parse и дополнительно проверяет expected kind.
- `IsValidSegment` используется для `package_id`/`mod_id` и contract-local keys, чья grammar явно равна `segment`.
- UE consumers обязаны использовать только thin UTF-8 adapter к `FStableId`; повторная grammar на `FString`, regex или ручном поиске separators запрещена.
- Utility возвращает `EStableIdError`; subsystem может преобразовать его в собственный diagnostic context, но не переопределяет результат validation.

## Namespace ownership

- `core` принадлежит основной игре.
- Namespace мода равен immutable `mod_id` из manifest.
- Package создаёт новые IDs только в собственном namespace.
- Package может полностью override существующий чужой ID, если он уже предоставлен более ранним provider.
- Создание нового ID в чужом namespace — `ForeignNamespaceNewId`.
- `runtime` и `system` зарезервированы и не используются модами.

`package_id`/`mod_id` — один `segment`, а не global Stable ID. Остальные публичные identities используют global grammar.

## Kind registry

`kind` определяет semantic family и expected schema. Core kinds включают:

```text
item, actor, quest, location, screen, command, event, module,
schema, text, resource, widget, slot, effect, operation, validator,
error, diagnostic
```

Package может добавить новый kind только вместе с declarative schema binding. Конфликтующие bindings одного kind/schema version являются fatal. Kind не выводится из directory.

## Typed references

Reference хранится полной canonical string. Expected kind задаётся schema поля или API signature.

```json5
{
  equipped_weapon_id: "rh:item.weapon.iron_sword",
  arrival_text_id: "rh:text.location.harbor.arrival",
}
```

Required reference должна разрешиться до repository publication. Optional reference проверяется так же, если поле присутствует. Short IDs и universal string marker `"none"` запрещены.

## Provider resolution

Для одинакового Stable ID:

1. Providers упорядочены package loader-ом: core, затем enabled mods.
2. Последний provider полностью заменяет definition.
3. Duplicate ID внутри одного package запрещён.
4. File order не влияет на winner.
5. Provenance хранит winning и shadowed providers.

## Lifecycle and publication freeze

Жизненный цикл Stable ID разделён на две фазы ([ADR-0023](../ADR/0023-stable-id-publication-freeze.md)):

1. **Pre-publication (Authoring / Draft)**: до публичного релиза идентификаторы не заморожены. Автор может свободно переименовывать определения на месте с помощью `gv2-content rename`, который атомарно обновляет определение и все ссылки на него в пределах пакета без создания редиректов.
2. **Publication Freeze**: наступает в момент публичного релиза пакета контента (поставка игрокам, публикация мода, фиксация эталонного манифеста). С этого момента Stable ID считается **опубликованным (Published)**.

## Redirects and tombstones

После публикации (Post-publication) действует безусловный инвариант: **опубликованный Stable ID никогда не переиспользуется для сущности с другим смыслом**. Любое переименование опубликованного ID обязано объявлять явный `redirect`, а удаление — `tombstone`. Повторное использование retired/tombstoned ID под новым активным определением запрещено (`PublishedIdReuse` / `active_definition_conflict`).

Redirect — explicit mapping `old_id -> new_id` для rename с сохранением логической непрерывности:

- Source и target имеют одинаковый kind.
- Redirect source не может одновременно быть active definition.
- Redirect source может объявить только owner его namespace. Мод не перенаправляет `core:*` IDs.
- Multiple targets одного source — `RedirectConflict`.
- Chain разрешается итеративно с visited set; cycle — fatal.
- Save после успешной load записывает final canonical target.
- Resolved package descriptor получает redirects/tombstones из manifest layer. Redirect source и tombstone не являются active definitions.
- Repository lookup flatten-ит chain для `Find`, но provenance хранит original ID и полный ordered chain.
- Tombstone не имеет target и возвращает typed tombstoned result при required lookup.

## Local child IDs

```text
local-child-id = stable-id "#" local-kind "." local-path
```

Примеры:

```text
core:quest.main#stage.intro
core:screen.shop#widget.buy_button
core:item.sword#slot.gem.primary
```

Local child уникален внутри `(owner_id, local_kind)`, не является global definition и существует только если owner schema объявляет соответствующий local kind.

## Persistent instance IDs

```text
instance-id = instance-kind "@" positive-counter
```

Примеры: `item@42`, `actor@7`, `quest@3`.

- Allocator принадлежит canonical save lineage.
- Counter начинается с 1, leading zeros запрещены.
- Выданная пара `(kind, counter)` никогда не переиспользуется.
- Next counters сохраняются.
- Singleton всё равно хранит отдельные `definition_id` и `instance_id`.

## Transient identities

```text
runtime@<session_generation>:<counter>
ui@<session_generation>:<counter>
```

Transient IDs не входят в canonical save, definitions и durable gameplay events. Generation mismatch возвращает `StaleTransientId` до registry lookup.

## Localization и resources

- `text_id` имеет kind `text`: `core:text.ui.inventory.title`.
- `resource_id` имеет kind `resource`: `core:resource.character.aria.casual`.
- Locale и UE path не входят в Stable ID.
- Mapping Stable ID → Primary Asset ID/Soft Object Path задаётся explicit presentation data.

## Module IDs

Lua module использует global Stable ID kind `module`:

```text
core:module.location_service
weather_mod:module.storm_rules
```

Filesystem path — provenance, но не identity.

## Errors

Минимальный набор stable codes:

```text
InvalidCharacter, NonAscii, InvalidSegmentStart, EmptySegment,
TooLong, WrongKind, UnknownKind, UnknownId, ForeignNamespaceNewId,
DuplicateDefinitionInPackage, RedirectConflict, RedirectKindMismatch,
RedirectCycle, PublishedIdReuse, InvalidLocalId, InvalidInstanceId,
StaleTransientId, ResourceMappingMissing
```

Diagnostic содержит package ID, package-relative source, JSON path, raw input, expected kind и provenance. Absolute filesystem path не показывается пользователю.

## Conformance

Обязательные tests покрывают grammar boundaries, mixed-case rejection, namespace ownership, full override, typed references, redirect chain/cycle, published-ID reuse rejection, local IDs, persistent allocator, stale transient IDs, localization/resources и deterministic indexes.

Grammar fixtures являются общими для UE и standalone host. Один и тот же `RunStableIdConformance` обязан выполняться Unreal automation-test и `gv2-headless --self-test`; отдельные копии positive/negative parser cases запрещены. Fixture минимум проверяет ASCII/Unicode, character set, segment start, empty/repeated segments, separators, maximum segment/ID length, parsed components и expected-kind mismatch.
