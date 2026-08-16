---
title: Состояние реализации
status: normative
version: 1.18
updated: 2026-08-15
depends_on:
  - ../README.md
  - ../Architecture/Overview.md
---

# Состояние реализации

> **Показывает:** что из нормативных contracts уже реализовано в коде.
> **Не является нормативным:** правил не вводит; при расхождении прав contract.
> **Обновляется:** в том же change set, который меняет объём реализованного.

Contracts в `Docs/Architecture` и `Docs/UI` описывают целевое поведение и являются нормативными. Этот документ отвечает на другой вопрос: **что из них уже реализовано в коде**. Он не вводит правил и при расхождении уступает contract.

Документ обязан обновляться в том же change set, который меняет объём реализованного.

## Легенда

- **Реализовано** — работает, покрыто tests, соответствует contract.
- **Частично** — работает ограниченный путь; остальная часть contract не реализована.
- **Не реализовано** — contract описан, кода нет.

## Content и repository

| Область | Статус | Комментарий |
|---|---|---|
| JSON5 parsing, source spans, bounded limits | Реализовано | Duplicate-key detection, UTF-8 validation, лимиты согласованы с Lua boundary |
| Definition envelope, schemas, explicit defaults | Реализовано | Scalar/container/union specs, extension schemas, semantic validators; схема `text` требует `source_message` (`min_length: 1`, [ADR-0022](../ADR/0022-external-translation-catalog.md)) |
| Namespace ownership, full override, redirects/tombstones | Реализовано | Включая chain/cycle и active-source conflicts |
| Typed references (`ref`, `text_id`, `resource_ref`) | Реализовано | Один recursive resolution path для `data` и extension blocks |
| Localization catalogs (PO) | Реализовано | Внешние PO-каталоги `<package-root>/localization/<locale>.po` ([ADR-0022](../ADR/0022-external-translation-catalog.md)), парсер `GV2ContentCore::ParsePo`, сборка String Table CSV `GV2ContentHostSupport::ExportPoToStringTableCsv`, изоляция от `content_hash` пакета |
| Provenance, minimal indexes, canonical hash | Реализовано | `ById`, `ByKind`, `ProvenanceById`, redirects/tombstones |
| Immutable snapshot и pinned read handle | Реализовано | `Find`/`Require`/`List`/`GetProvenance`/`GetContentHash` |
| Atomic publication, same-hash skip, session pinning | Реализовано | Synchronous, на Game Thread |
| Package discovery and ordering | Реализовано | Discovery набора пакетов по списку корней, назначение load_index, проверка зависимостей, циклов и load_after, генерация и сверка mods.lock.json5, FMultiPackageSourceProvider (план [PackageSupport](../Plans/PackageSupport/README.md), M1-M2 PKG-01–09 завершены) |
| Async candidate build, parallel workers, operation token | Не реализовано | Build и publish синхронные; token сведён к Game-Thread-проверке |

## Lua runtime

| Область | Статус | Комментарий |
|---|---|---|
| Lua 5.4.8 VM, one-VM invariant, owner thread | Реализовано | Wrong-thread и re-entry дают typed faults |
| Safe environment, module manifest/graph, protected entry | Реализовано | `load`/`loadfile`/`dofile`/`math.random` удалены |
| Value marshalling обоих portable типов | Реализовано | `FGV2LuaMarshaller`, единый path |
| `game.repository` (`get`/`require`/`list`/`exists`) | Реализовано | Detached deep copy, typed error codes, canonical `list` order |
| Semantic input ingress и command dispatch | Реализовано | Command Dispatcher с защитой от реентерабельности, открытие `mutation_window`, делегирование доменным методам сущностей/сервисам, команды `core:command.actor.reward` и `core:command.location.travel`, возврат structured result/error |
| `game.state`, canonical gameplay-state | Реализовано | Шесть секций (`meta`, `actors`, `item_instances`, `world`, `quests`, `mods`), `meta.player_actor_id`, аллокатор `instance_id` (`core:module.runtime.instance_allocator`), единая модель акторов, ссылочная целостность предметов, защита от висячих ссылок при удалении, окно мутации `mutation_window`, канонический хэш состояния в pure Lua (`core:module.runtime.state_hasher`), публикация скаляра через `GetCanonicalStateHash()` и включение в `FRunDigest` (план [CanonicalGameplayState](../Plans/Archive/CanonicalGameplayState/README.md), M1–M5) |
| `game.instances`, Runtime Instance Registry | Частично | Реализован реестр `game.instances.actors` (`get`/`exists`/`create`/`remove`/`ids`/`player`), disposable `ActorWrapper` с динамическим `discriminator` и доменными методами; singleton `game.instances.world()` возвращает disposable wrapper над `state.world`, не кэшируется, отклоняется валидатором state при попытке сохранения (план [GameplayEventsAndWorld](../Plans/Archive/GameplayEventsAndWorld/README.md), GEW-04); `state.world.current_location_id` (Stable ID kind `location`) валидируется grammar/kind/pinned-repository resolution, `GameData/core` содержит две локации (GEW-05); сервис `core:service.location` управляет переходами мира с публикацией фактов `leave`/`enter` (GEW-13, GEW-14) |
| `game.services`, Gameplay Services | Реализовано | Реестр `game.services` под `core:module.runtime.service_registry`, регистрация на фазе `register`, freeze после `register`, structured results; сервис `core:service.location` (`travel(target_id)`) |
| `game.commands`, validators, queue | Реализовано | Синхронный `command_dispatcher`, реестр `game.commands.validators` (`core:module.runtime.validator_registry`) с приоритетом/freeze/canonical order (GEW-01), read-only запуск до открытия mutation window (GEW-02), нормализация refusal envelope `{ code, params }` (GEW-03), валидатор `core:validator.location.travel` (GEW-13), очередь отложенных команд `game.commands.enqueue` ограниченной ёмкости (`MAX_COMMAND_QUEUE_SIZE = 100`) с исполнением в отдельных окнах мутации (план [GameplayEventsAndWorld](../Plans/Archive/GameplayEventsAndWorld/README.md), GEW-12) |
| `game.events` (EventBus), post-commit facts | Реализовано | Конверт событий (`core:module.runtime.event_envelope`, GEW-06), post-commit доставка и изоляция контекста (GEW-07), приоритет подписчиков и детерминированная breadth-first доставка (GEW-08), отслеживание runtime phases и pump limit breach (`core:module.runtime.event_bus`, GEW-09); реестр подписок `game.events.subscribers` (`core:module.runtime.subscriber_registry`) с freeze на фазе `register` (GEW-10); закрытое mutation window обработчиков и сохранение committed state при ошибке (GEW-11); этапы M3 и M4 закрыты |
| `game.random`, `game.time`, `game.log` | Не реализовано | — |
| Save/load, миграции, сериализация state | Реализовано | По ADR-0021 сериализация и миграции принадлежат Lua; от host-а требуется только slot-scoped byte storage (план [SaveAndLoad](../Plans/Archive/SaveAndLoad/README.md), M1–M5 SAV-01–21 завершены). Канонический кодек, slot-storage примитив, save path и cold start load реализованы: `FRuntimeSession::StartFromSave` читает слот host-примитивом до создания VM, `core:module.runtime.load` делает preflight конверта, разрешает redirect-цепочки произвольной длины и проверяет referential integrity (retired/unknown раздельными кодами) целиком в Lua; `state_hash` до сохранения и после загрузки совпадает в обоих host-ах. Версии секций (`core:module.runtime.migrate`) объявлены и записываются в конверт при каждом сохранении; хуки `migrate_state`/`restore_instances` подключены в module lifecycle между декодированием и присвоением состояния, downgrade и незаявленная миграция отклоняются раздельными типизированными ошибками. Явные per-section миграции сейвов написаны только как синтетическая проверка механизма (`GV2ColdStartLoadConformance`) — ни одна реальная секция ещё не меняла версию, поэтому production Scripts/ пока не содержит ни одного настоящего `migrate_state` |

## Hosts

| Область | Статус | Комментарий |
|---|---|---|
| UE bootstrap: image catalog, screen registry, repository publisher | Реализовано | Отказ любого из них блокирует создание VM с явным fault code |
| UE-native recovery surface | Реализовано | `UGV2RecoveryScreenWidget` при `Failed` |
| Session lifecycle | Частично | Реально проходятся `Creating`, `Ready`, `Failed` и завершение session; `Registering`/`BuildingState`/`RestoringInstances`/`Starting`/`PreparingPresentation` объявлены в enum, но не выставляются. Pending-slot, cancellation и Menu↔Game replacement отсутствуют |
| `gv2-headless` — parity gate | Реализовано | 24 portable conformance entry point плюс Lua-спеки исполняются обоими host-ами; host-локальное дублирование, встроенный Lua в production C++ и новые C++-проверки Lua-правил запрещены и проверяются CTest `host_conformance_parity_contract` |
| `gv2-headless` — deterministic replay | Реализовано | Run manifest, canonical SHA-256 run digest, переносимый `ReplayRunManifest`, CLI-флаги (`--manifest`, `--output-manifest`, `--output-digest`, `--check-scripts`), golden fixtures и cross-host digest parity в CTest и Unreal Automation (тот же план, M2 завершён); golden-прогон пинится к замороженному тестовому корпусу (`Tests/Fixtures/PortableContentCore/valid/core`), а не к живому `GameData/core` — рост игрового контента не меняет golden (план [TestArchitectureAndLuaSpecs](../Plans/Archive/TestArchitectureAndLuaSpecs/README.md), TAS-08) |
| Lua Spec Runner (`Tests/Lua/`) | Реализовано | Формат спеки, обнаружение, исполнение и идентичность провала `<spec>.<case>`; `GV2TestSupport::RunLuaSpecs` вызывается обоими host-ами. Под-дерево определяет сессию: `Tests/Lua/world/` — продакшн-сессия, `Tests/Lua/commands/` — изолированная fixture-сессия. Проверка Lua-правила больше не требует C++ |
| Миграция conformance на спеки | Частично | Мигрированы наборы world, command validators и module lifecycle (удалено 7701 строк C++). Унаследованными остаются `GV2LuaRepositoryConformance` (489) и `GV2ValidatorRegistryConformance` (332); список закрыт, расширение запрещено гейтом |
| Независимость тестов от контента | Реализовано | `GameData/core` и замороженный корпус `Tests/Fixtures/PortableContentCore/valid/core` — разные деревья; golden-прогон строится на корпусе; каждое pinned-значение имеет один источник; переписи корпуса в тестах заменены на утверждения свойств |
| `gv2-content` CLI и Live Loop | Реализовано | `validate` (включая `--watch`), `inspect`, `describe`, `new`, `refs`, `rename` (с проверкой `package_frozen`), `index`, `hash`, `coverage` (отчёт о полноте перевода PO-каталогов), стабильные exit codes (план [ContentAuthoringTools](../Plans/Archive/ContentAuthoringTools/README.md), M1–M4); модульная архитектура CLI с разделением на изолированные команды в `Tools/Content/Source/Commands/` и сервисы поддержки в `Tools/Content/Source/Support/` (план [ContentCliModularization](../Plans/Archive/ContentCliModularization/README.md), M1–M4); интеграция с редакторами (`.vscode/tasks.json` с problemMatcher, `.vscode/settings.json`, `Tools/Editor/generate_vscode_snippets.py`, `Tools/Editor/README.md`) |
| Cross-host parity | Реализовано | Один corpus, один pinned hash, проверяется CI |

## Presentation

| Область | Статус | Комментарий |
|---|---|---|
| Blueprint Screen Templates и Screen Registry | Реализовано | Валидация `screen_id`, layers, duplicates, concrete classes |
| Native widget bases | Реализовано | Text, RichText, Image, Button, ButtonList, Checkbox, InputField, DropdownSelect, ProgressBar, Separator, LoadingIndicator, RichTextPopover |
| Screen Field Adapter Registry | Реализовано | Stateless mapping `schema_id → adapter` |
| Централизованный UI theme и text pipeline | Реализовано | — |
| Image Resource Catalog | Реализовано | `fixed_aspect`/`nine_slice`/`tile`, filesystem discovery |
| Semantic input и UI binding registry | Реализовано | Bounded FIFO ingress, session-scoped bindings, stale-handle rejection |
| UI document: routes, layers, overlays, modals | Не реализовано | Активен один screen за раз |
| Presentation effects (one-shot) | Не реализовано | — |
| Локализация по locale | Реализовано | Lua публикует неразрешённый `TextSpec` DTO; Presentation хоста резолвит `TextSpec` через `UGV2TextPipeline` и `FText::Format` по PO/StringTable каталогам темы с прозрачным fallback на `source_message` ([ADR-0022](../ADR/0022-external-translation-catalog.md), план [LocalizationPipeline](../Plans/Archive/LocalizationPipeline/README.md), M1–M4) |

## Ближайшие разрывы

Приоритет определяется тем, что блокирует end-to-end vertical slice из [Architecture Overview](../Architecture/Overview.md):

1. UI document reconciliation (routes, layers, overlays) — нужен, как только экранов станет больше одного.
2. Mod package lifecycle — discovery, load order и `mods.lock`; ядро override/redirect уже умеет, но ни один host не грузит больше одного package root.
3. `game.random` и `game.time` — детерминированные PRNG и часы; слоты в `meta` зарезервированы, генераторов нет.

Save/load (план [SaveAndLoad](../Plans/Archive/SaveAndLoad/README.md)) закрыт целиком, M1–M5, и больше не входит в этот список.
