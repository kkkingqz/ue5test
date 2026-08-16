---
title: Modding Architecture
status: draft
version: 0.8
updated: 2026-08-16
depends_on:
  - StableIDSpecification.md
  - DefinitionEnvelopeAndSchemaRules.md
  - LuaRuntimeContract.md
---

# Modding Architecture

> **Владеет:** границами мод-пакета, тем, что мод может расширять, и trust model расширений.
> **Не владеет:** механикой сборки репозитория и разрешения override ([GameDataRepository](GameDataRepositoryContract.md)).
> **Инварианты:** [INV-009](Invariants.md), [INV-010](Invariants.md)
> **Реализация:** манифест пакета (M1) и discovery набора пакетов, порядок загрузки, валидация зависимостей, циклов, `load_after` и `mods.lock.json5` (M2) реализованы (план [PackageSupport](../Plans/Archive/PackageSupport/README.md)); Lua из пакетов и save metadata остаются future work (M3–M4). См. [Implementation Status](../Status/ImplementationStatus.md).
Mod — trusted content package с одним immutable `mod_id`, который одновременно является его namespace.

## Игровой пакет поставки (`rh`)

Базовая поставка игры разделена на два пакета (план [RhGamePackage](../Plans/Archive/RhGamePackage/README.md)):

- `core` (движок) — схемы (`schemas/`), экраны (`core:screen.*`), команды, события, базовые модули среды исполнения (`Scripts/`). Движок не содержит конкретных игровых сущностей и не знает идентификаторов пространства `rh:`.
- `rh` (игра) — конкретные игровые сущности: предметы (`rh:item.*`), персонажи/акторы (`rh:actor.*`), локации (`rh:location.*`), а также привязанные к ним тексты (`rh:text.*`), переводы (`ru.po`) и ресурсы (`rh:resource.*`).

**Архитектурное правило разделения:** конкретные сущности живут в `rh`, механизмы и возможности — в `core`. Запрещены любые обратные ссылки из `core`, `Scripts/` или `Source/` на пространство имён `rh:` (проверяется гейтом `core_decoupling_gate_contract`).

## Package contents

```text
manifest             # package.json5 — mandatory (PKG-01, план PackageSupport)
definitions/
schemas/            # only for new kinds/extension sites
scripts/
locales/
resources/
optional cooked Pak
```

**Manифест реализован (PKG-01/02/03, план [PackageSupport](../Plans/Archive/PackageSupport/README.md)).** `package.json5` обязателен: пакет без него, с невалидным JSON5 или с отсутствующими/некорректными identity-полями отвергается диагностикой (`core:diagnostic.package.manifest.*`) до чтения `definitions/`/`schemas/`. Identity больше не выводится из имени каталога.

```json5
{
  package_id: "weather_mod",
  namespace: "weather_mod",   // обязано совпадать с package_id (v1: один пакет — один namespace)
  version: "1.2.0",           // <major>.<minor>.<patch>, неотрицательные целые
  compatibility: {             // необязательно; отсутствующая ось всегда совместима
    game: { min: 1, max: 1 },
    api: { min: 1, max: 1 },
    schema: { min: 1, max: 1 },
  },
  dependencies: [               // необязательно; форма проверяется здесь (PKG-03),
    { package_id: "core_extras", load_after: true },  // присутствие в наборе — M2 (PKG-06)
  ],
  redirects: { /* ... */ },
  tombstones: [ /* ... */ ],
}
```

`compatibility.{game,api,schema}` — каждая ось `{min, max}` целых чисел, сверяется с текущими версиями build-а (`GV2ContentHostSupport::Current{Game,Api,Schema}Version`); несовместимый диапазон отвергает пакет диагностикой `core:diagnostic.package.manifest.incompatible_range`, называющей и требуемый диапазон, и фактическую версию. `core` не объявляет `compatibility` вовсе — движок всегда совместим сам с собой.

`dependencies[].load_after` — подсказка редактору порядка, не меняющая runtime order (см. «Load order» ниже); зависимость проверяется на присутствие в наборе и на циклы отдельным этапом (M2 Discovery and Order), здесь — только форма записи.

`module graph`, `content roots` и `optional Pak metadata` в манифесте — по-прежнему future work следующих milestones плана PackageSupport (M2/M4), не M1.

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
- Foreign ID считается override только если более ранний provider уже предоставил exact ID; иначе candidate получает `core:diagnostic.repository.identity.foreign_new_id`.
- Override является одной complete entry: `data`, tags, `deprecated` и extensions предыдущего provider не наследуются и не merge-ятся.
- Invalid override блокирует candidate целиком; fallback к shadowed definition запрещён.
- Core redirect объявляет core; mod не перенаправляет чужой namespace.
- Mod может объявить redirect/tombstone только для source ID собственного namespace; redirect target может быть same-kind foreign ID.
- Published IDs не переиспользуются.
- New kind требует declarative schema binding.
- Extension block использует собственный package namespace и exact registered extension schema для `definition_file`, `definition_entry` либо `schema_resource`; mod не переопределяет основную schema существующего kind.

## Lua modules

- Module ID: `weather_mod:module.storm_rules`.
- Модули мода поставляются в подкаталоге `scripts/` пакета мода. Имя источника атрибутируется пакетом: `@<package_id>/<relative>` (например `@weather_mod/gameplay/storm_rules.lua`).
- Dependencies и модули объявляются манифестом пакета (`scripts/manifest.lua`).
- Мод может объявлять новые модули только в своём namespace (попытка объявить новый модуль в чужом namespace без предшествующего провайдера отклоняется ошибкой `LuaModuleForeignNewId`).
- Module возвращает export table и не создаёт globals.
- Таблицы экспорта модулей неизменяемы после загрузки (`LuaModuleExportFrozen`); ad-hoc мутация чужих экспортов запрещена ([ADR-0025](../ADR/0025-lua-module-replacement-and-export-freezing.md)).
- Замещение модулей разрешено только для явно помеченных как замещаемые (`replaceable: true`, например `gameplay/`, `debug/`); модули ядра запечатаны по умолчанию (`LuaModuleSealed`).
- Замещающий модуль получает доступ к базовой реализации через `require_base()` во время инициализации.
- Хуки жизненного цикла (`register`, `validate_state` и др.) вызываются только у активного победителя.
## Package commands

- Мод регистрирует свои команды в хуке жизненного цикла `M.register(ctx)` своего модуля через `game.commands.handlers.register("pkg:command.name", handler_fn)`.
- Идентификаторы команд принадлежат собственному namespace мода (`<mod_id>:command.<path>`).
- **Защита от случайного перекрытия**: повторная регистрация существующего `command_id` без явного флага `override` отклоняется ошибкой `CommandHandlerDuplicateRegistration` и блокирует запуск сессии. Правило «поздний пакет побеждает» для команд сознательно не применяется (в отличие от declarative definitions): механику нельзя тихо подменить порядком загрузки пакетов.
- **Явное перекрытие**: мод может осознанно переопределить существующую команду ядра или другого мода, передав `options.override = true`.
- **Защита от ложного перекрытия**: попытка указать `options.override = true` для идентификатора, который ещё не был зарегистрирован предшествующим пакетом, вызывает ошибку `CommandHandlerOverrideMissing`.
- Команды мода исполняются через стандартный конвейер: валидаторы мода (`game.commands.validators`), окно мутации, публикация фактов (`game.events.enqueue`) и реакция подписчиков (`game.events.subscribers`). Никаких правок ядра или C++ для добавления команд мода не требуется.

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
