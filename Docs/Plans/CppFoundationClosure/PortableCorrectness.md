---
title: Cpp Foundation Portable Correctness
status: active
version: 1.0
updated: 2026-09-12
depends_on:
  - ../../Architecture/HeadlessSimulationContract.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../../Architecture/CanonicalStateAndSave.md
  - ../../Architecture/LuaRuntimeContract.md
---

# Portable correctness и Lua ownership

> **Материализует:** подтверждённые REVIEW-04…07 из внешнего C++ review в рамках [C++ Foundation Closure](README.md). Сверка и границы выводов находятся в [текущем аудите](../../Status/AuditFindings.md); отдельного проекта исправлений нет.

## CFC-03A — Замкнуть канонические numbers и validation hash-полей

- [ ] CFC-03A — Замкнуть канонические numbers и validation hash-полей

**Зависимость:** CFC-03. **Файлы:** `Source/GV2ContentCore/Private/Value.cpp`, `Source/GV2ContentCore/Private/CanonicalHash.cpp`, существующие value/hash conformance; `Source/GV2RuntimeCore/Private/GV2RunManifest.cpp`, `GV2RunDigest.cpp`, `GV2RunManifestConformance.cpp`, `GV2RunDigestConformance.cpp` в том же каталоге. Общий strict SHA-256 text validator разместить в `Source/GV2ContentCore/Public/GV2ContentCore/CanonicalHash.h` и его implementation; оба codecs зависят от ContentCore уже сейчас. Docs: `DefinitionEnvelopeAndSchemaRules.md`, `HeadlessSimulationContract.md`, `BuildAndTooling.md`; при изменении golden — `Docs/Guides/RegenerateGolden.md`.

**Инвариант:** [canonical numbers](../../Architecture/DefinitionEnvelopeAndSchemaRules.md), [manifest/digest](../../Architecture/HeadlessSimulationContract.md). Equal finite Number zeros имеют одну canonical representation; одинаковые hash fields проходят одинаковый domain validator. NaN/Infinity уже отвергает `FValue(double)` — дублирующий parser в hasher не нужен.

**Решение:** нормализовать zero в единственном double-construction path `FValue`; `MakeNumber` делегирует ему. Integer 0 и Number 0.0 сохраняют разные kinds. `IsCanonicalSha256(std::string_view)` принимает ровно 64 lowercase ASCII hex characters; typed diagnostics остаются у каждого codec. StateHash может быть пустым только в явно разрешённом contract исходе, это не расширяет grammar непустого hash. Actual hash-field inventory — declarations manifest/digest + обращения их serializers/deserializers; expected field policy задаётся независимо contract fixture.

**Не считается закрытием:** normalize только JSON5 parser; заменить hash golden результатом текущего бага; reject NaN только в hash вместо уже существующего value guard; проверять лишь длину строки; выводить expected acceptance из того же validator.

**Шаги:**
1. В shared conformance закрепить воспроизведённые `FValue(0.0) == FValue(-0.0)` при разных hashes и принятие Digest с 64 символами `z` при отказе Manifest.
2. Нормализовать constructor value, проверить прямой constructor, MakeNumber, copy/move и вложенные array/object значения; NaN/±Infinity по-прежнему отвергаются до hash. Inventory construction paths вывести из public constructors/factories и parser/builder callers, а не из ручного перечня тестов.
3. Подключить общий hash validator к каждому hash-bearing manifest/digest input. Fixtures проверяют lowercase success, uppercase, nonhex ASCII, UTF-8, длины 0/63/65, permitted empty StateHash и запрещённый empty required hash. Сопоставить actual field inventory с отдельной expected policy; unknown field policy делает gate красным.
4. Выполнить negative mutations zero normalizer и одного codec call; оба ломают shared tests через public APIs. Проверить serialized fixtures, golden compatibility и typed refusal для ранее принимаемых malformed records. Обновление golden обосновать canonical rule, не подгонкой.
5. Прогнать shared conformance в UE/headless, portable CTest, docs validator; удалить STATUS-024/025 только после production codec checks.

**Done:**
- Direct/parser/builder Number zero имеет одну representation/hash, сохраняя отличие Integer от Number.
- Non-finite construction по-прежнему отвергается; исходное утверждение review о принятии NaN не переносится в новую норму.
- Actual hash fields codecs полностью сопоставлены с независимой domain policy; missing validator обнаруживается.
- Каждая negative mutation приводит к ожидаемому failure через публичный constructor/codec.
- Изменение hashes/accepted inputs классифицировано по CompatibilityPolicy; malformed input не получает silent normalization.

**Evidence:** red/green public API probes, independent input corpus, hash-field/construction inventory, mutation outputs, оба host-а и docs validation. Закрывает CFC-AF-06/07, STATUS-024/025.

## CFC-05A — Передать сборку canonical state целиком Lua

- [ ] CFC-05A — Передать сборку canonical state целиком Lua

**Зависимость:** CFC-05. **Файлы:** `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, public session header; `Scripts/runtime/state_validator.lua`, `Scripts/bootstrap/main.lua`, `Scripts/bootstrap/manifest.lua`; создать `Scripts/runtime/state_composition.lua`, `Tests/Lua/lifecycle/state_contributions.lua`; перенести затронутые Lua-rule assertions из `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp` в shared Lua specs. Native mechanism tests сохраняют проверки stack/error/phase boundary. Docs: `CanonicalStateAndSave.md`, `LuaRuntimeContract.md`, `BootstrapAndSessionLifecycle.md`, `Docs/Guides/AddLuaModule.md`.

**Инвариант:** [INV-013 / C++ scope](../../Architecture/Overview.md#границы-c), [canonical state](../../Architecture/CanonicalStateAndSave.md). C++ не выбирает section names, merge policy и mod namespace ownership; decoded/contribution trees остаются внутри Lua VM.

**Решение:** fixed Lua composition entry point вызывает module state hooks по уже resolved load order и собирает temporary tree до assign. Module ID, ordered sources и scalar start inputs могут поступать от host; таблицы contributions не marshalled в C++. `MergeStateContribution`, `IsCanonicalStateSection`, special cases `meta`, `mods`, counters/PRNG/time и collision exceptions переносятся в Lua. Нативный lifecycle вызывает один protected entry point и читает outcome; generic Lua-driven merge descriptor с ключами state в C++ запрещён. State composition policy не меняет full override definitions.

**Не считается закрытием:** заменить строки C++ enum-ом; перенести только три вложенных ключа, сохранив native root/mod rules; создать generic C++ merge engine; очистить indentation без устранения ownership drift.

**Шаги:**
1. Записать independent fixtures valid contributions, collisions, forbidden mod namespace, unknown section, invalid type/metatable и failure последнего module. Проверить реальные Start/StartFromSave boundaries: failed temporary tree не назначается canonical state.
2. Перенести semantic policy и module contribution execution в Lua; сохранить contract order/error semantics либо явно классифицировать correction существующего расхождения. Generic native stack/type checks остаются только как безопасность вызова, без знания canonical sections.
3. Вывести actual state-semantic native references из Lua C API access sites session implementation; gate допускает fixed boundary handles `game`/`state` и generic type/lifetime operations, но запрещает schema-shaped traversal/merge. Не ограничивать gate только строками `prng`/`time`: неизвестный semantic state access требует классификации. Expected ownership policy независима от source inventory.
4. Добавить namespaced state contribution и его validation только Lua-файлами; доказать отсутствие нового native branch по production diff и shared lifecycle spec. Вернуть native merge path в negative fixture — gate и ownership scenario обязаны обнаружить обход Lua owner.
5. Исправить REVIEW-15 indentation в затронутом lifecycle block, не делать отдельный style-only этап. Обновить contracts/Guide, shared Lua specs в обоих hosts, native stack/fault tests, `--check-scripts`, docs validation.

**Done:**
- Canonical section/collision/mod merge semantics принадлежат одному Lua owner; C++ не интерпретирует contribution tree.
- State build faults не назначают partial state и сохраняют native stack/context; проверено public start path.
- Новая Lua contribution проходит оба host-а без нового native gameplay/state branch.
- Actual native state-access inventory и negative bypass gate подключены к штатной приёмке.
- STATUS-022 закрыт только после удаления semantic native path; CFC-AF-05 получает исход, CFC-AF-15 — запись о сопутствующем форматировании.

**Evidence:** Lua policy fixtures и hook traces, source inventory, native bypass mutation, Lua-only extension diff, UE/headless results. Перенос кода сам по себе не доказывает одинаковую семантику ошибок.

## CFC-07A — Передать seed через единый deterministic session input

- [ ] CFC-07A — Передать seed через единый deterministic session input

**Зависимость:** CFC-05A и CFC-07; выполняется до save/load CFC-08…10. **Файлы:** `Source/GV2RuntimeCore/Private/GV2RunReplay.cpp`, `GV2RuntimeSession.cpp`, manifest/digest codecs и conformance в том же каталоге; public session/manifest headers; `Headless/Source/main.cpp`; coordinator/start descriptor CFC-07; `Scripts/bootstrap/main.lua`, `Scripts/runtime/state_composition.lua`, `Scripts/runtime/state_validator.lua`; создать `Scripts/runtime/random.lua`, `Tests/Lua/lifecycle/deterministic_seed.lua`. Docs: `HeadlessSimulationContract.md`, `CanonicalStateAndSave.md`, `LuaRuntimeContract.md`, `RuntimeFacadeAndRegistries.md`, `Docs/Authoring/LuaGameplayReference.md`.

**Инвариант:** [deterministic inputs](../../Architecture/LuaRuntimeContract.md#determinism-and-technical-ingress), [replay](../../Architecture/HeadlessSimulationContract.md). Seed — input игры, session generation — lifetime token. Их нельзя подменять друг другом; одинаковый manifest с тем же seed воспроизводит один authoritative результат в обоих hosts.

**Решение:** typed session start inputs несут seed отдельно от generation и доступны Lua до первого default/start hook. C++ передаёт точный scalar seed, не реализует PRNG и не изменяет `meta.prng`. Lua engine module владеет named deterministic streams и их canonical save state. Алгоритм, stream-name derivation и golden vectors фиксируются owner contract CFC-01 до кода; случайное поведение не делегируется отключённому `math.random`. Полный uint64 transport не проходит через double: boundary использует фиксированную 16-символьную lowercase hex string. Manifest/digest encoding для полного uint64 явно версионируется; legacy nonnegative numeric seed в поддержанном диапазоне мигрируется без потери, остальные значения получают typed refusal. Контракт uint64 и фактический ограниченный JSON int64 parser не оставляются в противоречии.

**Не считается закрытием:** заменить `Start(1, ...)` на `Start(Manifest.Seed, ...)`; включить seed только в DigestHash; вызвать seed setup command после случайных bootstrap hooks; реализовать PRNG в C++; назвать отсутствие seed transport доказанным текущим randomized divergence без сценария.

**Шаги:**
1. Через реальный ReplayRunManifest вызвать Lua fixture, использующий start seed до выполнения первой accepted command; fixture проверяет ожидаемое значение/последовательность, а не только metadata digest. Исходный API seed не передаёт.
2. Протащить typed start inputs через actual Start/StartFromSave/replay callers; обязательные inputs и compile errors запрещают silent default на одном host. Library convenience defaults допускаются только с явной документированной семантикой, gameplay profile фиксирует seed в descriptor/manifest.
3. Реализовать Lua-owned deterministic stream initialization и serializable state. Повтор одного seed даёт golden sequence; разные seeds дают заранее заданные разные fixture outputs. Не требовать математически невозможного отсутствия любых collisions для всех uint64.
4. Проверить полный uint64 domain boundary (0, INT64_MAX, INT64_MAX+1, UINT64_MAX), codec migration/refusal и generation independence. Actual startup paths вывести из callers/closed mode enum; expected seed/stream vectors задаются независимо реализации.
5. В CFC-10 LoadSave восстанавливает сохранённые stream states, а не reseed нового прохождения; Restart использует committed descriptor. Добавить save/load/continue stream sequence в CFC-12. Мутация игнорирования seed или reseed on load обязана краснеть.
6. Выполнить shared conformance/specs и replay в обоих hosts, синхронизировать contracts/Authoring и compatibility version. STATUS-023 удалить после production replay, не после сериализации поля.

**Done:**
- Seed достигает Lua до bootstrap/default state и не зависит от session generation.
- Lua-owned PRNG имеет зафиксированные алгоритм и vectors; C++ не хранит gameplay stream state.
- Manifest → runtime replay проверяет фактическую seeded последовательность/state hash, не лишь digest metadata.
- Startup-path inventory покрывает UE/headless/replay modes; full uint64 transport точен.
- Load/continue semantics связаны с задачами CFC-10/12 и не допускают скрытого reseed.
- STATUS-023/CFC-AF-04 закрываются с уточнением исходной находки: отсутствовал seed path, а не доказанный default-PRNG crash/divergence.

**Evidence:** startup path inventory, independent seeded vectors, codec boundary corpus, replay/state assertions двух hosts, red-on-revert, Load/continue evidence CFC-10/12 для окончательной freeze-приёмки.
