---
title: Image Resource Lookup Optimization Proposal
status: archived
proposal_state: implemented
version: 0.2
updated: 2026-08-13
depends_on:
  - ../../Architecture/SystemContextAndComponents.md
  - ../../UI/ImageResources.md
decisions:
  - ../../ADR/0016-png-suffix-image-metadata.md
  - ../../ADR/0017-centralized-ui-presentation-paths.md
---

# Предложение по оптимизации lookup Image Resource Catalog

> **Предлагает:** immutable O(1) lookup и однократную подготовку resolved brush.
> **Затрагивает:** [Image Resources](../../UI/ImageResources.md).
> **Состояние:** реализовано; нормативный результат перенесён в contracts, документ сохраняется как rationale.

Реализовано 2026-08-13. Нормативное runtime-поведение зафиксировано в
[`ImageResources.md`](../../UI/ImageResources.md); этот документ сохраняет исходное
обоснование и критерии реализации.

## Назначение и область

Предлагается заменить повторную validation, линейный поиск и повторное построение `FSlateBrush` в `UGV2ImageResourceCatalog::Resolve()` на immutable lookup, подготовленный при successful candidate build.

Предложение не меняет discovery, filename grammar, render modes, staging или resource readiness semantics.

## Оценка текущего состояния

| Область | Текущее состояние | Вывод |
|---|---|---|
| Discovery | Recursive sorted scan реализован | Сохранить |
| Metadata | `.png`, `.tile.png` и `.9.png` валидируются при build | Сохранить |
| Candidate publication | Configured catalog заменяется только после successful build | Сохранить |
| Lookup | `Resolve()` вызывает `Validate()` и `Entries.FindByPredicate()` | Оптимизировать |
| Brush preparation | `ResolveDefinition()` заново строит brush при каждом lookup | Подготовить один раз |

## Ownership и источник истины

- `Resources/<namespace>/resource/<path>.png` остаётся authoring source.
- `UGV2ImageResourceCatalog` владеет application-level immutable mapping `resource_id → resolved texture/render metadata`.
- `UGV2ImageResourceCatalogSettings` владеет atomic replacement configured catalog.
- `FGV2ImagePresentation` остаётся единственным runtime path от `resource_id` к mutation `UImage`.

Catalog не становится частью GameDataRepository. Lua и portable DTO продолжают содержать только `resource_id`.

## Инварианты

- `Resolve(resource_id)` не перечисляет entries и не выполняет catalog-wide validation.
- PNG decode, metadata validation и brush preparation выполняются до publication candidate.
- Failed candidate не меняет current configured catalog.
- Lookup key проходит strict Stable ID validation без normalization.
- Runtime content image применяет только centralized image presentation path.
- Внутренний cache immutable после publication.

## Предлагаемая модель

Catalog получает private transient storage:

```text
ResolvedById: TMap<FString, FGV2ResolvedImageResource>
RuntimeTextures: TArray<TObjectPtr<UTexture2D>>
```

`BuildFromDirectory` обязан:

1. Deterministically enumerate sources.
2. Вывести и строго проверить `resource_id`.
3. Decode image и вычислить render metadata.
4. Создать transient texture.
5. Один раз подготовить `FGV2ResolvedImageResource`.
6. Отклонить duplicate ID или invalid entry.
7. Сформировать complete candidate map и texture ownership set.
8. Заменить внутреннее состояние только после полного success.

После publication `Resolve()` выполняет strict ID validation, `ResolvedById.Find()` и копирование prepared value.

`Entries` может сохраняться как ordered diagnostic/inspection representation, но не участвует в runtime lookup. Soft texture reference внутри filesystem-built entry не должен оставаться вторым resolution path.

## Failure и recovery

| Failure | Поведение |
|---|---|
| Invalid path, Stable ID, PNG или marker | Candidate отклоняется целиком |
| Duplicate derived `resource_id` | Candidate отклоняется целиком |
| Texture или brush preparation failure | Candidate не публикуется |
| Unknown/invalid ID при resolve | Typed presentation failure; current catalog не меняется |

Optional placeholder policy остаётся в Presentation и не реализуется скрытой подменой внутри catalog.

## Польза, риски и трудоёмкость

- **Польза:** average `O(1)` lookup; validation и brush preparation исчезают из повторного Widget apply.
- **Трудоёмкость:** **S**.
- **Риск stale cache:** map строится целиком как часть candidate, а не изменяется поэлементно.
- **Риск дублирования memory:** после перехода следует удалить runtime-дублирование, не нужное inspection API.
- **Риск hardware-dependent benchmark:** acceptance сравнивает scaling, а не абсолютный микросекундный порог.

## Не входит в предложение

- Новый render mode или filename grammar.
- Изменение packaged paths или staging.
- Deferred loading, async operations или eviction.
- Live rescan active session.
- Raw texture/brush/path через Lua boundary.

## Критерии приёмки

- `Resolve()` не вызывает `Validate()`, `FindByPredicate()`, PNG decode или brush preparation.
- Lookup 10, 1 000 и 10 000 synthetic entries не демонстрирует линейного роста относительно catalog size в benchmark profile.
- Repeated resolve возвращает эквивалентные render mode, size и margins.
- Failed rebuild сохраняет прежний configured catalog и его lookups.
- Tests покрывают duplicate ID, unknown/invalid ID и все три render mode.
