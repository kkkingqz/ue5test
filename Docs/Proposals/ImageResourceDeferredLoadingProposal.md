---
title: Image Resource Deferred Loading Proposal
status: draft
proposal_state: measurement_required
version: 0.1
updated: 2026-08-13
depends_on:
  - ../Architecture/CommandsAndEvents.md
  - ../Architecture/LuaRuntimeContract.md
  - ../UI/ImageResources.md
  - ../UI/PresentationSnapshotAndEffects.md
decisions:
  - ../ADR/0005-value-only-async-boundary.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0016-png-suffix-image-metadata.md
---

# Предложение по deferred loading Image Resources

## Назначение и область

Предлагается рассмотреть разделение startup metadata index и загрузки texture payload по требованию. Реализация разрешена только после измерения representative catalog и подтверждения, что eager decode/create даёт неприемлемый startup time или resident memory.

До прохождения gate действует текущая простая модель: все PNG валидируются, декодируются и превращаются в transient textures при catalog build.

## Текущее состояние и gate

`UGV2ImageResourceCatalog::BuildFromDirectory()` сейчас:

1. Enumerates все PNG.
2. Полностью декодирует каждый image.
3. Проверяет mode metadata и `.9.png` marker border.
4. Создаёт `NeverStream` transient `UTexture2D`.
5. Публикует catalog только после полного success.

Перед началом реализации обязаны быть измерены:

- representative resource count;
- total compressed и decoded bytes;
- cold catalog-build/startup duration;
- peak и resident texture memory;
- first-screen resource working set;
- target platform budgets.

Без зафиксированного превышения budget proposal остаётся `measurement_required`.

## Ownership и границы

- Metadata index и prepared texture cache принадлежат UE Presentation/Application lifetime.
- Filesystem I/O и image decode не пересекают Lua boundary.
- UObject/texture creation выполняется только на разрешённом UE thread.
- Lua запрашивает typed operation по `resource_id` и получает только Technical Input result.
- C++ не хранит Lua callback; completion проверяет session generation и operation token.
- Headless использует metadata-only catalog и не участвует в UE texture loading.

## Предлагаемый lifecycle

```text
startup metadata validation
  → immutable metadata index publish
  → prepare(resource_ids)
  → deduplicated async read/decode
  → UE-thread texture creation
  → candidate prepared-cache commit
  → Technical Input completion
  → Presentation apply or command retry
```

### Startup metadata validation

- Stable ID, duplicate ID, suffix и basic PNG metadata проверяются до publication.
- Для `.9.png` marker geometry обязана быть проверена до resource становится доступным. Если это требует decode, implementation может сохранить compact validated metadata и освободить pixels до prepare.
- Invalid required source блокирует candidate catalog; ошибка не откладывается до случайного первого отображения.

### Prepare operation

- Concurrent prepare одного `resource_id` deduplicates в одну underlying operation.
- Result различает prepared, unknown, invalid, failed и stale.
- Required resource следует existing `resource_not_ready`/retry contract.
- Prefetch advisory и не создаёт gameplay authority.
- Partial batch success policy должна быть explicit в operation schema; silent partial success запрещён.

### Cache lifetime

Первый вариант после measured approval использует session/application-lifetime cache без eviction. LRU, automatic unloading и pin counts добавляются только при отдельной измеренной memory problem, потому что они существенно усложняют brush/Widget lifetime.

## Failure и recovery

| Failure | Поведение |
|---|---|
| Invalid metadata при startup | Candidate metadata index не публикуется |
| Read/decode failure при prepare | Typed operation failure; cache не получает partial entry |
| Texture creation failure | Typed operation failure после UE-thread cleanup |
| Stale generation/token | Completion discard без Presentation mutation |
| Required resource not prepared | Apply/command следует documented not-ready path |
| Optional resource unavailable | Presentation применяет только documented type-compatible fallback |

## Compatibility и обязательные документы

Deferred loading меняет observable readiness/lifecycle Image Resources. До кода необходимо:

1. Обновить `ImageResources.md` полным metadata/prepare/cache contract.
2. Уточнить operation DTO и retry semantics в `LuaRuntimeContract.md`, `CommandsAndEvents.md` и `PresentationSnapshotAndEffects.md`.
3. Создать ADR, если меняется startup publication, resource lifetime или session ownership invariant.
4. Добавить exact operation/error IDs и schema fixtures.

## Польза, риски и трудоёмкость

- **Потенциальная польза:** меньше startup work и resident texture memory для большого catalog.
- **Трудоёмкость:** **L**.
- **Риск:** сложная readiness state machine без реальной выгоды. Мера — measured gate.
- **Риск:** UObject/thread misuse. Мера — explicit worker/UE-thread handoff и tests.
- **Риск:** stale completion после restart. Мера — generation/token validation.
- **Риск:** repeated decode или request storm. Мера — deduplication и prepared cache.
- **Риск:** premature eviction. Мера — no eviction в первом варианте.

## Не входит в первый вариант

- LRU eviction, texture atlas или virtual textures.
- Background UObject creation.
- Live filesystem rescan или hot resource replacement.
- Gameplay branching по факту physical texture load.
- Raw operation handle, texture или callback в canonical state/save.

## Критерии допуска к реализации

- Representative measurements и platform budgets приложены к proposal или benchmark report.
- Зафиксировано измеренное превышение startup или memory budget.
- Согласованы operation schema, batch failure policy, ownership и cache lifetime.
- Приняты необходимые ADR и contract updates.

## Критерии приёмки реализации

- Startup публикует полностью validated immutable metadata index без создания всех textures.
- Duplicate concurrent prepare выполняет один underlying load.
- Texture creation и cache commit происходят на разрешённом UE thread.
- Required resource проходит typed not-ready → prepare → Technical Input → full revalidation flow.
- Stale completion после restart не меняет cache, UI или gameplay-state.
- Headless build не получает UE media dependency.
- Representative benchmark подтверждает достижение target budget без regression first-screen latency сверх согласованного предела.
