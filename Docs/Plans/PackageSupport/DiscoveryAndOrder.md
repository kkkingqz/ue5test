---
title: Discovery and Order Tasks
status: draft
version: 1.0
updated: 2026-08-16
depends_on:
  - PackageManifest.md
  - ../../Architecture/GameDataRepositoryContract.md
  - ../../Proposals/ModPackageLifecycleProposal.md
---

# M2 — Discovery and Order

> **Материализует:** [Modding § Load order](../../Architecture/Modding.md), [GameDataRepository Contract](../../Architecture/GameDataRepositoryContract.md).
> **Задачи:** PKG-05…09.
> **Результат:** репозиторий собирается из набора пакетов в явном, воспроизводимом порядке.

## Результат этапа

Хост обнаруживает набор пакетов, проверяет их совместно, выстраивает в явный пользовательский порядок и передаёт упорядоченный набор дескрипторов в `BuildRepository`. Override между пакетами работает end-to-end во всех трёх хостах.

## Задачи

- [ ] **PKG-05 — Обнаружение набора корней**
  - Зависимости: PKG-01.
  - Сейчас `DiscoverPackageFromDirectory` вызывается ровно один раз каждым хостом, а `load_index` жёстко равен `0`.
  - Done: появляется host-support функция обнаружения набора по списку сконфигурированных корней; `load_index` присваивается по фактическому порядку; порядок перечисления каталогов файловой системой не влияет на результат; дубликат `package_id` — диагностика с обоими путями.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-06 — Явный порядок и проверка зависимостей**
  - Зависимости: PKG-03, PKG-05.
  - Done: порядок задаётся конфигурацией пользователя и не переставляется рантаймом скрыто; отсутствующая зависимость, цикл и нарушение объявленного `load_after` — раздельные диагностики; `load_after` остаётся подсказкой и не меняет порядок сам; `core` всегда первый.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-07 — Lock-файл**
  - Зависимости: PKG-06.
  - Done: генерируется `mods.lock.json5` с `schema_version`, списком пакетов, версиями и fingerprints; файл является generated application data — не редактируется Lua и не является source definitions; одинаковые манифесты и порядок дают побайтово одинаковый lock; расхождение lock и фактического набора — типизированная диагностика, а не тихая перегенерация.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-08 — Перевести все три хоста на набор**
  - Зависимости: PKG-05.
  - Done: UE (`GV2FilesystemContentSourceProvider`), `gv2-headless` и `gv2-content` передают упорядоченный набор дескрипторов вместо одного; одиночный корень остаётся частным случаем набора из одного элемента; ни один хост не собирает набор собственной логикой обхода каталогов.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-09 — Фикстура реального мода и синхронизация документации**
  - Зависимости: PKG-08.
  - `Tests/Fixtures/PortableContentCore/valid/test_mod` сегодня содержит только `definitions/` и используется как источник override на уровне ядра.
  - Done: фикстура становится полноценным вторым пакетом с манифестом; conformance-набор покрывает full override definition из мода, redirect собственного namespace, попытку создать `core:*` ID (`foreign_new_id`) и невалидный override, блокирующий кандидат целиком; набор исполняется обоими хостами; [Modding](../../Architecture/Modding.md) и [Implementation Status](../../Status/ImplementationStatus.md) обновлены.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Репозиторий собирается из двух пакетов; результат не зависит от порядка обхода файловой системы.
- [ ] Смена пользовательского порядка меняет победителя override предсказуемо.
- [ ] Дубликат `package_id`, цикл зависимостей и `foreign_new_id` дают раздельные типизированные диагностики.
- [ ] `mods.lock.json5` воспроизводится побайтово при неизменных входах.
- [ ] Все три хоста используют один путь обнаружения.
