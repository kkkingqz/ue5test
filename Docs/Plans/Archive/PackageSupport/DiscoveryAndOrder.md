---
title: Discovery and Order Tasks
status: archived
version: 1.0
updated: 2026-08-16
depends_on:
  - PackageManifest.md
  - ../../../Architecture/GameDataRepositoryContract.md
  - ../../../Proposals/ModPackageLifecycleProposal.md
---

# M2 — Discovery and Order

> **Материализует:** [Modding § Load order](../../../Architecture/Modding.md), [GameDataRepository Contract](../../../Architecture/GameDataRepositoryContract.md).
> **Задачи:** PKG-05…09.
> **Результат:** репозиторий собирается из набора пакетов в явном, воспроизводимом порядке.

## Результат этапа

Хост обнаруживает набор пакетов, проверяет их совместно, выстраивает в явный пользовательский порядок и передаёт упорядоченный набор дескрипторов в `BuildRepository`. Override между пакетами работает end-to-end во всех трёх хостах.

## Задачи

- [x] **PKG-05 — Обнаружение набора корней**
  - Зависимости: PKG-01.
  - Сейчас `DiscoverPackageFromDirectory` вызывается ровно один раз каждым хостом, а `load_index` жёстко равен `0`.
  - Done: появляется host-support функция обнаружения набора по списку сконфигурированных корней `DiscoverPackagesFromDirectories`; `load_index` присваивается по фактическому порядку; порядок перечисления каталогов файловой системой не влияет на результат; дубликат `package_id` — диагностика `core:diagnostic.package.discovery.duplicate_package_id` с обоими путями.
  - Evidence: `GV2ContentHostSupport::DiscoverPackagesFromDirectories`, `GV2ContentHostSupport::FMultiPackageSourceProvider`, тест `GV2ContentHostSupport::Testing::RunPackageDiscoveryAndOrderConformance()`.

- [x] **PKG-06 — Явный порядок и проверка зависимостей**
  - Зависимости: PKG-03, PKG-05.
  - Done: порядок задаётся конфигурацией пользователя и не переставляется рантаймом скрыто; отсутствующая зависимость (`core:diagnostic.package.order.missing_dependency`), цикл (`core:diagnostic.package.order.dependency_cycle`) и нарушение объявленного `load_after` (`core:diagnostic.package.order.load_after_violation`) — раздельные диагностики; `load_after` остаётся подсказкой и не меняет порядок сам; `core` всегда первый (`core:diagnostic.repository.package_set.missing_core`, `invalid_core_index`).
  - Evidence: `GV2ContentCore::ValidatePackageDescriptors`, тесты в `RunPackageDiscoveryAndOrderConformance()`.

- [x] **PKG-07 — Lock-файл**
  - Зависимости: PKG-06.
  - Done: генерируется `mods.lock.json5` с `schema_version: 1`, списком пакетов, версиями, `load_index` и sha256 fingerprints; файл является generated application data — не редактируется Lua и не является source definitions; одинаковые манифесты и порядок дают побайтово одинаковый lock; расхождение lock и фактического набора — типизированная диагностика `core:diagnostic.package.lock.mismatch`, а не тихая перегенерация.
  - Evidence: `GV2ContentHostSupport::ComputePackageFingerprint`, `GenerateModsLockContent`, `VerifyModsLock` (`Source/GV2ContentHostSupport/Public/GV2ContentHostSupport/ModsLock.h` + `Private/ModsLock.cpp`), тесты в `RunPackageDiscoveryAndOrderConformance()`.

- [x] **PKG-08 — Перевести все три хоста на набор**
  - Зависимости: PKG-05.
  - Done: UE (`GV2FilesystemContentSourceProvider`), `gv2-headless` и `gv2-content` передают упорядоченный набор дескрипторов вместо одного; одиночный корень остаётся частным случаем набора из одного элемента; ни один хост не собирает набор собственной логикой обхода каталогов.
  - Evidence: `BuildGV2RepositoryFromDirectories`, `Headless/Source/main.cpp` `BuildRepositoryFromDirectories`, `PackageLoader::BuildFromPackageRoots`, `ValidateCommand` поддержка мульти-корней.

- [x] **PKG-09 — Фикстура реального мода и синхронизация документации**
  - Зависимости: PKG-08.
  - `Tests/Fixtures/PortableContentCore/valid/test_mod` сегодня содержит только `definitions/` и используется как источник override на уровне ядра.
  - Done: фикстура становится полноценным вторым пакетом с манифестом `valid/test_mod/package.json5`; conformance-набор покрывает full override definition из мода, redirect собственного namespace, попытку создать `core:*` ID (`foreign_new_id`) и невалидный override, блокирующий кандидат целиком; набор исполняется обоими хостами; [Modding](../../../Architecture/Modding.md) и [Implementation Status](../../../Status/ImplementationStatus.md) обновлены.
  - Evidence: `Tests/Fixtures/PortableContentCore/valid/test_mod/package.json5`, `fixtures.index`, `GV2.Runtime.ContentCore.PackageDiscoveryAndOrderConformance`, `gv2-headless --self-test`.

## Проверка milestone

- [x] Репозиторий собирается из двух пакетов; результат не зависит от порядка обхода файловой системы.
- [x] Смена пользовательского порядка меняет победителя override предсказуемо.
- [x] Дубликат `package_id`, цикл зависимостей и `foreign_new_id` дают раздельные типизированные диагностики.
- [x] `mods.lock.json5` воспроизводится побайтово при неизменных входах.
- [x] Все три хоста используют один путь обнаружения.
