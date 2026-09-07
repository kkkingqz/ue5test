---
title: Package Set Tasks
status: active
version: 1.1
updated: 2026-09-07
depends_on:
  - README.md
  - ContractAlignment.md
  - ../../Architecture/HeadlessSimulationContract.md
---

# M1 — Package Set

> **Материализует:** `PAH-R3`, `PAH-R6` и `D1/D5` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md).
> **Задачи:** PSC-02…03.
> **Результат:** package set выводится один раз, а полный manifest входит в package identity без UE-зависимости portable-слоя.

## Зафиксированный portable тип

```text
GV2ContentHostSupport::FResolvedPackageSource
  Root: filesystem path used only by builders
  Descriptor: immutable FPackageDescriptor
  CanonicalManifestHash: 64 lowercase hex

GV2ContentHostSupport::FResolvedPackageSet
  OrderedSources: immutable load-order sequence
```

Canonical manifest hash вычисляется из полного разобранного JSON5 root до проекции известных полей в `FPackageDescriptor`. Форматирование и комментарии на hash не влияют; любое semantic field, включая неизвестное будущему descriptor host extension, влияет.

## Задачи

- [ ] **PSC-02 — Ввести один `FResolvedPackageSet`**
  - Зависимости: PSC-01.
  - Инвариант: repository, Lua source loader и UE presentation builders потребляют один value, построенный до них. Повторное package discovery ниже bootstrap является вторым авторитетом, даже если сейчас даёт тот же порядок.
  - Не считается закрытием: перестановка `LoadScreenRegistry()`; сравнение независимо выведенных наборов; сохранение public downstream helpers, повторно открывающих canonical `GameData`; отдельные ветки consumers для Editor/test roots.
  - Done:
    - `FResolvedPackageSet` и `FResolvedPackageSource` принадлежат `GV2ContentHostSupport` и не зависят от Unreal;
    - production lock, Editor profile, automation fixture и Headless CLI различаются только factory-входом, а не consumer path;
    - repository builder получает ordered descriptors, Lua loader — ordered roots/sources, presentation candidate builder — тот же set целиком;
    - Screen Registry и schema/resource builders не вызывают package discovery самостоятельно;
    - legacy core script fallback, если сохраняется из-за layout `Scripts/`, разрешается внутри source для уже выбранного `core` package и не может добавить package вне set;
    - число мест создания set выводится по функциям, возвращающим `FResolvedPackageSet`, из declarations; разрешены только host bootstrap entry points;
    - constructor/factory inventory имеет negative self-test: synthetic downstream factory/call отвергается;
    - production scenario с Editor roots, отличными от `mods.lock`, подтверждает единый порядок repository, Lua, schemas, screens и resources;
    - Headless использует те же portable factories и не получает Unreal/presentation link edge.
  - Evidence: `Source/GV2ContentHostSupport/Public/`, `Source/GV2ContentHostSupport/Private/`, `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, `Source/GV2/Private/Application/GV2SessionCoordinator.cpp`, `Headless/Source/main.cpp`, новый declaration-derived gate.

- [ ] **PSC-03 — Зафиксировать canonical manifest identity**
  - Зависимости: PSC-02.
  - Инвариант: package fingerprint покрывает полное semantic содержимое manifest, но Headless run digest покрывает только portable gameplay inputs/results. Эти identity намеренно различны.
  - Не считается закрытием: добавление `ue_content_roots` в старый список полей; source inventory известных descriptor members; hash сырых bytes, меняющийся от пробелов/комментариев; включение package fingerprint или presentation data в `FRunDigest`.
  - Done:
    - package discovery вычисляет `CanonicalManifestHash` из полного parsed `FValue` до извлечения известных полей;
    - `ComputePackageFingerprint` использует descriptor identity и `CanonicalManifestHash`, а не повторный перечень manifest fields;
    - изменение форматирования/комментариев сохраняет hash, изменение любого semantic field меняет hash;
    - synthetic неизвестное host-extension field меняет manifest hash и package fingerprint без изменения `FPackageDescriptor` API;
    - `ue_content_roots` меняет package fingerprint и regenerated `mods.lock.json5`;
    - `repository_content_hash`, `package_fingerprint`, `presentation_hash`, `session_content_id` и `FRunDigest` остаются разными typed concepts;
    - изменение только UE-specific field/asset не меняет `repository_content_hash`, `script_set_hash`, state hash или Headless run digest;
    - portable CTest и UE host проверяют одинаковый package order/fingerprint из одного conformance implementation.
  - Evidence: `Source/GV2ContentHostSupport/Private/PackageDiscovery.cpp`, `Source/GV2ContentHostSupport/Private/ModsLock.cpp`, portable conformance, `GameData/mods.lock.json5`, `Docs/Architecture/HeadlessSimulationContract.md`.

## Проверка milestone

- [ ] Set создаётся только разрешёнными host bootstrap entry points; перечислитель выведен из return type, не из имени функции.
- [ ] Все consumers получают один immutable set; downstream rediscovery отсутствует.
- [ ] Arbitrary semantic manifest field меняет fingerprint без ручного обновления перечня.
- [ ] Headless использует тот же resolver, остаётся UE-free и сохраняет прежний run digest.
