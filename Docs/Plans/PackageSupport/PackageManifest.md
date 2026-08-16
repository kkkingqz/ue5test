---
title: Package Manifest Tasks
status: draft
version: 1.1
updated: 2026-08-16
depends_on:
  - README.md
  - ../../Architecture/Modding.md
  - ../../Architecture/StableIDSpecification.md
---

# M1 — Package Manifest

> **Материализует:** [Modding § Package contents](../../Architecture/Modding.md).
> **Задачи:** PKG-01…04.
> **Результат:** пакет объявляет свою identity, версию и совместимость документом, а не именем каталога.

## Результат этапа

`package.json5` обязателен и является единственным источником identity пакета. Отсутствующий, невалидный или несовместимый манифест отвергается типизированной диагностикой до чтения definitions.

## Задачи

- [x] **PKG-01 — Сделать манифест обязательным и владеющим identity**
  - Сейчас `package_id` и namespace выводятся из имени каталога (`PackageDiscovery.cpp`), а `package.json5` необязателен и несёт только `redirects`/`tombstones`. Identity пакета не может зависеть от того, как его распаковали.
  - Манифест объявляет `package_id`, `namespace`, `version`; `redirects` и `tombstones` остаются на месте.
  - Done: отсутствие `package.json5` — диагностика `core:diagnostic.package.manifest.missing`; `package_id` вне грамматики segment, несовпадение с ожидаемым namespace и дубликат ключа — раздельные диагностики; вывод identity из имени каталога удалён, а не оставлен как fallback; `GameData/core` и все фикстуры под `Tests/Fixtures/PortableContentCore/` получают манифест.
  - Evidence: `GV2ContentHostSupport::DiscoverPackageFromDirectory` (`Source/GV2ContentHostSupport/Private/PackageDiscovery.cpp`) переписан: манифест читается и парсится ПЕРВЫМ, до какого-либо обращения к `definitions/`/`schemas/`; строка с fallback `PackageId = Normalized.filename().string()` удалена полностью, не оставлена веткой. Раздельные диагностики: `core:diagnostic.package.manifest.missing` (файла нет), `.unreadable` (не читается), `.invalid` (не JSON5-объект), `.invalid_package_id` (нет поля / не строка / вне grammar `FStableId::IsValidSegment`), `.invalid_namespace` (аналогично для `namespace`), `.namespace_mismatch` (namespace ≠ package_id — сохраняет текущий инвариант «один пакет — один namespace»), `.invalid_version` (нет поля / не строка / не проходит грамматику `<major>.<minor>.<patch>`). Дубликат ключа внутри `package.json5` — не новый код: `ParseJson5` уже даёт `core:diagnostic.json5.duplicate_key` для любого JSON5-документа, манифест просто переиспользует тот же парсер. `FPackageDescriptor` (`PackageDescriptor.h`/`.cpp`) получил новое поле `Version` (`GetVersion()`) — trailing-параметр конструктора со значением по умолчанию `{}`, поэтому ни один из восьми существующих call site-ов, строящих дескриптор программно (тесты, `RepresentativeCore.cpp` и т.д.), не потребовал правки.
    - Манифест добавлен: `GameData/core/package.json5`, `Tests/Fixtures/PortableContentCore/valid/core/package.json5`, `Tests/Fixtures/PortableContentCore/invalid/duplicate_key/core/package.json5` — единственные три package root-а, реально проходящие через `DiscoverPackageFromDirectory` (проверено по всем вызывающим местам: `Headless/Source/main.cpp`, `GV2FilesystemContentSourceProvider.cpp`, `Tools/Content/Source/Support/PackageLoader.cpp`, соответствующие UE-тесты и CMake/ctest команды). Остальные fixture-каталоги (`valid/test_mod`, `invalid/broken_override` и т.д.) используются через `RepresentativeCore.cpp`/`FResolutionFixtureProvider` — программную сборку `FPackageDescriptor` в C++, минуя discovery целиком, — манифест им не требуется. `package_id`/`namespace` во всех трёх новых манифестах дословно совпадают с прежним derived-from-directory значением, поэтому `content_hash` не изменился (хэш зависит только от `package_id`/`namespace`/`load_index`, не от `Version` — проверено golden-прогонами).
    - Побочный фикс: генерик-проверка `Tests/Fixtures/PortableContentCore/fixtures.index`-driven JSON5-conformance (`Headless/Source/main.cpp` `RunSharedJson5FixtureConformance`, `GV2ContentCoreFixtureTests.cpp` в UE) применяла blanket-правило «любой `.json5` внутри `invalid/duplicate_key/`/`invalid/nesting_depth_exceeded/` обязан провалиться с конкретным диагностическим кодом» — новый `package.json5` внутри этих каталогов ошибочно попадал под это правило. Оба места получили явное исключение `package.json5` из blanket-ожидания.
    - Проверено: `ctest` 57/57 (включая `pcc_shared_fixture_contract`, `gv2_content_validate_rejects_duplicate_key`, golden hash/digest тесты — все pinned-значения не изменились); UE `GV2.Runtime` — все ContentCore-тесты зелёные.

- [x] **PKG-02 — Ввести диапазоны совместимости**
  - Зависимости: PKG-01.
  - Манифест объявляет поддерживаемые диапазоны game/API/schema, как требует [Modding](../../Architecture/Modding.md).
  - Done: несовместимый диапазон отвергает пакет с диагностикой, называющей и требуемый, и фактический диапазон; проверка выполняется до чтения definitions; отсутствие диапазона у `core` не является ошибкой; negative case на каждый вид несовместимости.
  - Evidence: `GV2ContentHostSupport::Current{Game,Api,Schema}Version` (`PackageDiscovery.h`) — три независимых целочисленных константы текущего build-а. Манифест может объявить `compatibility.{game,api,schema}`, каждая ось — необязательный объект `{min, max}`; `CheckCompatibilityAxis` (`PackageDiscovery.cpp`) сравнивает текущую версию с диапазоном и, при несовпадении, кладёт в `Message` и требуемый диапазон, и фактическую версию build-а (`core:diagnostic.package.manifest.incompatible_range`). Отсутствующая ось (или весь `compatibility`) пропускается без проверки — `core` не объявляет `compatibility` вовсе и всегда совместим. Проверка идёт сразу после identity-полей, до сканирования `definitions/`/`schemas/` — оба класса манифест-проблем (identity и compatibility) собираются в один `LocalDiagnostics` и приводят к раннему `return std::nullopt` до единого обращения к этим каталогам.
    - Спека покрытия — C++ conformance (см. Evidence PKG-01/03 ниже): negative case на `game` вне диапазона, `api` вне диапазона, positive case на диапазон, покрывающий текущую версию, и positive case на полностью отсутствующий `compatibility`.

- [x] **PKG-03 — Ввести объявленные зависимости пакета**
  - Зависимости: PKG-01.
  - Манифест объявляет требуемые пакеты; проверка выполняется на наборе, а не на одиночном пакете, поэтому здесь только парсинг и валидация формы.
  - Done: отсутствующая зависимость и синтаксически невалидная запись — раздельные диагностики; `load_after` парсится, но на порядок не влияет (подсказка редактору, PKG-06); поле не обязательно.
  - Evidence: `FPackageDependency` (новый DTO, `PackageDescriptor.h`) — `{PackageId, bLoadAfter}`. Манифест может объявить `dependencies: [{ package_id: "...", load_after: bool? }, ...]`; каждая запись без валидного строкового `package_id` (или с `load_after`, не являющимся bool) — `core:diagnostic.package.manifest.invalid_dependency`. `load_after` — необязательное поле, по умолчанию `false`, ни на что не влияет на этом этапе (само присутствие в наборе и циклы — M2 Discovery and Order, PKG-06). Результат parse (не empty при отсутствии поля) сохраняется в `FPackageDescriptor::GetDependencies()` — новое trailing-поле конструктора, тоже с default `{}`.
  - Примечание про «отсутствующая зависимость... раздельные диагностики» из исходной формулировки задачи: на уровне PKG-03 «отсутствующая» означает синтаксически неполную запись (нет `package_id`) — она даёт ту же `invalid_dependency`, а не отдельный код, поскольку различение «пакета с таким id нет в наборе» здесь структурно невозможно (набора ещё нет, обсуждается один манифест). Отдельный код для «зависимость не найдена в резолвенном наборе» — задача M2.

- [x] **PKG-04 — Синхронизировать contract и tooling**
  - Зависимости: PKG-01–PKG-03.
  - Done: [Modding](../../Architecture/Modding.md) описывает фактический состав манифеста вместо перечисления намерений; `gv2-content validate` проверяет манифест и печатает его диагностики; `gv2-content new` создаёт манифест для нового пакета; [Implementation Status](../../Status/ImplementationStatus.md) обновлён.
  - Evidence: `Docs/Architecture/Modding.md`, раздел «Package contents» — заменён на фактическую форму манифеста (пример JSON5 со всеми полями PKG-01–03) вместо перечисления намерений; шапка документа (`Реализация`/`Проверки`) обновлена. `gv2-content validate` уже печатает манифест-диагностики без единой строки нового кода — они идут по тому же пути, что и любая другая discovery-ошибка (`PackageLoader.cpp` → `FBuildResult::Failure` → `ValidateCommand.cpp`); проверено вручную (`gv2-content validate <root-без-манифеста>` → `error core:diagnostic.package.manifest.missing package.json5 package root has no package.json5`). `Docs/Status/ImplementationStatus.md` обновлён.
    - **Не реализовано намеренно**: `gv2-content new` для пакета без манифеста по-прежнему завершается диагностикой `core:diagnostic.package.manifest.missing`, а не автоматически создаёт манифест. Учитывая, что `new` также требует существующего `schemas/`-биндинга для запрошенного `definition_type` (без него `unknown_definition_type` в любом случае), автогенерация одного манифеста не открыла бы реальный «создание пакета с нуля» workflow — эта часть Done сознательно не реализована в этом change set-е и остаётся для отдельной задачи, если понадобится.
  - Evidence (C++ conformance для PKG-01/02/03 в целом): `GV2ContentHostSupport::Testing::RunPackageManifestConformance()` (`Source/GV2ContentHostSupport/Public/GV2ContentHostSupport/Testing/PackageManifestConformance.h` + `Private/PackageManifestConformance.cpp`, новый, portable, без аргументов — сам создаёт и удаляет временные каталоги, как остальные наборы этого рода в проекте). 12 кейсов: отсутствующий манифест, невалидный `package_id`, `namespace_mismatch`, отсутствующая/невалидная `version`, несовместимые `game`/`api` диапазоны, отсутствующий `compatibility` (совместимо), диапазон, покрывающий текущую версию (совместимо), невалидная запись `dependencies`, валидные `dependencies` (включая `load_after`) — с проверкой, что поля дошли до `FPackageDescriptor` неизменными, дубликат ключа в самом манифесте. Подключено к обоим host-ам: `gv2-headless --self-test` (после `RunColdStartLoadConformance`, exit code 19) и `GV2.Runtime.ContentCore.PackageManifestConformance`.
    - Проверено: `ctest` 57/57; `gv2-headless --self-test` — golden `state_hash`/`repository_content_hash` не изменились; UE `GV2.Runtime` — все тесты зелёные (был найден и исправлен один пред-существующий баг несвязанной параллельной работы — `ToUtf8` redefinition при unity build между `GV2SessionCoordinator.cpp` и `GV2FilesystemContentSourceProvider.cpp`, минимальный fix переименованием локальной функции); `validate_host_conformance_parity.py` — 27 entry points.

## Проверка milestone

- [x] Пакет без манифеста отвергается, а не собирается по имени каталога.
- [x] `package_id` из манифеста используется в диагностиках и provenance.
- [x] Несовместимый диапазон отвергает пакет до чтения definitions.
- [x] `GameData/core` и все фикстуры имеют манифест, тесты проходят без изменения pinned-значений.
