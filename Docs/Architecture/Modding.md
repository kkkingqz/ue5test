---
title: Modding Architecture
status: draft
version: 0.1
updated: 2026-08-10
depends_on:
  - StableIDSpecification.md
  - DefinitionEnvelopeAndSchemaRules.md
  - LuaRuntimeContract.md
---

# Modding Architecture

Mod — trusted content package с одним immutable `mod_id`, который одновременно является его namespace.

## Package contents

```text
manifest
definitions/
schemas/            # only for new kinds/extension sites
scripts/
locales/
resources/
optional cooked Pak
```

Manifest объявляет version, compatible game/API/schema ranges, dependencies, module graph, content roots и optional Pak metadata.

## Load order

1. Core.
2. Enabled mods в явном user order после dependency validation.
3. Duplicate mod ID и dependency cycle — fatal.
4. `load_after` может помочь editor-у предложить order, но runtime не меняет user order скрыто.
5. File order внутри package не является semantics.

## Identity and content

- New IDs только в namespace мода: `weather_mod:item.ring.storm`.
- Existing foreign ID можно fully override.
- New `core:*` ID из mod запрещён.
- Core redirect объявляет core; mod не перенаправляет чужой namespace.
- Published IDs не переиспользуются.
- New kind требует declarative schema binding.
- Extension block использует собственный package namespace и registered extension schema.

## Lua modules

- Module ID: `weather_mod:module.storm_rules`.
- Dependencies объявляются manifest-ом.
- Module возвращает export table и не создаёт globals.
- Public extension surface: `game.mods.weather_mod`.
- Commands, validators, events, services и lifecycle hooks регистрируются до freeze.
- Event handler меняет gameplay только через queued command.

Все enabled modules проходят full Menu и Game session lifecycle. Bootstrap error включённого mod блокирует candidate session с diagnostic mod/source.

## Trust model

Mod Lua code trusted относительно gameplay-state и может повредить его при нарушении contract. При этом public API не выдаёт filesystem, process, native libraries, debug, raw UObject, Blueprint reflection или callback pointers.

Separate hostile-code sandbox, signatures, process isolation и quotas — future architecture, не обещание v1.

## Presentation assets

Mod без Pak использует существующие `widget_id`, slots и resource types. Новый Widget Blueprint/asset class требует cooked Pak, собранный совместимым Mod Kit и mounted до repository build. Hot unmount не поддерживается.

Lua и definitions используют `resource_id`, не UE paths. Missing optional resource получает typed fallback; required resource блокирует operation.

## Save compatibility

Save metadata хранит enabled mods, order, versions и fingerprints. Disabled/missing mod state остаётся opaque orphaned section. Re-enable требует compatibility check и module migration before restore.

Удаление mod может оставить missing definitions; affected state/reference policy обязана быть explicit. Runtime не перепривязывает ID к похожему core object автоматически.

## Failure policy

- Invalid definition/schema/extension блокирует candidate repository целиком.
- Module compile/register/start error блокирует candidate session.
- Runtime command/event fault переводит session в `Failed` по общим rules.
- Diagnostics содержат mod ID, package-relative source, Stable ID, schema version и correlation IDs.

## Initial authoring checklist

- Unique lowercase `mod_id`.
- Compatible API/schema ranges.
- No foreign new IDs.
- Full override содержит complete valid entry.
- Module dependencies declared and acyclic.
- No globals/raw asset paths.
- State stored in common registries or own `mods[mod_id]` section.
- Commands/events use canonical IDs and schemas.
- Fixtures cover enable/disable, override, save orphan/restore и missing resources.

