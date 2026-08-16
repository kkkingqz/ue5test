---
title: Build and Tooling Contract
status: normative
version: 2.3
updated: 2026-08-15
depends_on:
  - SystemContextAndComponents.md
  - GameDataRepositoryContract.md
  - HeadlessSimulationContract.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0018-portable-content-core-module.md
  - ../ADR/0019-content-host-support-module.md
  - ../ADR/0022-external-translation-catalog.md
  - ../ADR/0023-stable-id-publication-freeze.md
  - ../ADR/0024-lua-spec-runner.md
---

# Build and Tooling Contract

> **Владеет:** составом build-таргетов, исполняемыми хостами, конвенцией package root, кодами возврата, общими фикстурами и составом CI.
> **Не владеет:** поведением рантайма — его определяют подсистемные contracts.
> **Инварианты:** [INV-012](Invariants.md), [INV-013](Invariants.md)
> **Реализация:** `Source/CMakeLists.txt`, `*.Build.cs`, `Tools/Content/`, `.github/workflows/linux-ci.yml`.
> **Проверки:** `pcc_shared_fixture_contract`, `host_conformance_parity_contract`, `gv2_content_*`.

Документ фиксирует, как один и тот же source set собирается двумя build systems, какие исполняемые host-ы существуют, где живут shared test fixtures и что обязан проверить integration gate. Ownership и dependency direction задаёт [System Context and Components](SystemContextAndComponents.md); здесь описан только physical build/tooling слой.

## Ownership


- `Source/CMakeLists.txt` — единственное место объявления portable CMake targets.
- `*.Build.cs` — единственное место объявления Unreal Build Tool modules.
- Оба build systems компилируют одни и те же файлы из `Source/`. Копия portable source внутри host-каталога запрещена.
- Tooling никогда не становится runtime dependency: ни один gameplay host не линкует CLI-код.

## Portable targets

| CMake target | UBT module | Содержимое | Зависит от |
|---|---|---|---|
| `gv2_content_core` | `GV2ContentCore` | Value model, Stable ID, JSON5, schemas, repository build/snapshot | — |
| `gv2_content_host_support` | `GV2ContentHostSupport` | Filesystem package discovery (ADR-0019); Lua spec file discovery (TAS-02, ADR-0024) | `gv2_content_core` |
| `gv2_runtime_core` | `GV2RuntimeCore` | Lua 5.4.8 VM, runtime session, `FGV2LuaMarshaller`, slot-scoped save storage primitive (SAV-05/06, план [SaveAndLoad](../Plans/Archive/SaveAndLoad/README.md)) | `gv2_content_core` |
| `gv2_test_support` | `GV2TestSupport` | Lua spec runner orchestration (TAS-04, ADR-0024); test-only, ни один gameplay host не линкует | `gv2_content_host_support`, `gv2_runtime_core` |
| — | `GV2` | UE composition, Bridge, Presentation | все четыре |

Vendored Lua (`Source/GV2RuntimeCore/Private/ThirdParty/Lua54`) собирается только внутри `gv2_runtime_core` и не выставляется через public headers.

### Save slot storage primitive (SAV-05/06/07, план [SaveAndLoad](../Plans/Archive/SaveAndLoad/README.md))

`GV2RuntimeCore::ISaveSlotStorage` (`Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2HostServices.h`) — единственный C++ примитив плана SaveAndLoad (ADR-0021): чтение и запись непрозрачных байт по `save_slot_id`, с типизированным результатом (`Ok`/`NotFound`/`Unreadable`/`Failure`). Интерфейс не содержит путей, `FString`, UObject и filesystem-типов; отсутствие конкретной реализации не мешает `FRuntimeSession::Start` — примитив не является параметром сессии, как и `IResourceCatalog`/`ILocalizationAdapter` рядом с ним.

`GV2RuntimeCore::FFilesystemSaveSlotStorage` — единственная реализация примитива, используемая обоими host-ами без дублирования: `std::filesystem::path` уже принят как portable-тип на этом уровне (см. discovery-заголовки `GV2ContentHostSupport`). Каждый host передаёт конструктору свой корневой каталог; резолв `save_slot_id` в путь и его ограничение этим каталогом целиком внутри реализации. Запись идёт во временный файл рядом со слотом и публикуется одним `rename` (атомарным на одном volume) — отказ на любом шаге до `rename` оставляет предыдущий опубликованный слот нетронутым.

Conformance-набор `GV2RuntimeCore::Testing::RunSaveSlotStorageConformance()` (`Source/GV2RuntimeCore/Public/GV2RuntimeCore/Testing/GV2SaveSlotStorageConformance.h`) сам создаёт и удаляет временный каталог — оба host-а вызывают его без аргументов и без host-specific setup, как остальные наборы в этом namespace. Покрывает write/read roundtrip с произвольными байтами (включая NUL), чтение отсутствующего слота, чтение слота с не-файлом на его месте, прерванную запись с сохранением предыдущего содержимого и отказ адресации по невалидному `save_slot_id`. Исполняется `gv2-headless --self-test` и `GV2.Runtime.SaveAndLoad.SaveSlotStorageConformance`.

## Executable hosts

Три host-а строят repository одним `BuildRepository()` path и одной discovery-конвенцией; собственной schema/resolution логики у них нет.

| Host | Каталог | Назначение | Lua VM |
|---|---|---|---|
| `GV2` (UE) | `Source/GV2` | Игра и Unreal automation | Да |
| `gv2-headless` | `Headless/` | Deterministic gameplay без UE | Да |
| `gv2-content` | `Tools/Content/` | Content validation и inspection | Нет |

`Tools/Content` намеренно вынесен из `Content/` — последний является Unreal asset root.

### Package root convention

Package root — это один каталог, чьё имя равно `package_id` и обязано быть canonical `segment`:

```text
<package-root>/
  package.json5          optional manifest: redirects, tombstones, freeze flags
  definitions/*.json5    definition files
  schemas/*.json5        self-describing schema resources: id, definition_type, schema_version
  localization/*.po      optional translation catalogs: <locale>.po (ADR-0022)
```

`GV2ContentHostSupport::DiscoverPackageFromDirectory()` — единственная реализация сканирования пакета; она сортирует файлы, выдаёт `core:diagnostic.package.discovery.*` при ошибках и возвращает `FPackageDescriptor`. Discovery пакета намеренно не сканирует каталог `localization/`: переводы хранятся во внешних PO-каталогах, ключуются Stable ID kind `text` (`text_id`), не меняют `FPackageDescriptor` и не влияют на `content_hash` репозитория. Мод поставляет переводы по той же структуре внутри своего package root. Multi-package root, mod discovery и load order не поддерживаются.

`package.json5` необязателен: его отсутствие эквивалентно пакету без retirement-таблиц и без заморозки. Он является manifest layer из [GameDataRepository Contract](GameDataRepositoryContract.md) и объявляет:

| Поле | Значение |
|---|---|
| `redirects` | Объект `source_id → target_id`; source и target одного kind |
| `tombstones` | Массив retired ID |
| `frozen` / `published` | Пакет считается опубликованным: `gv2-content rename` отказывается переписывать ID на месте и требует объявить redirect ([ADR-0023](../ADR/0023-stable-id-publication-freeze.md)) |

Ни один существующий пакет манифеста пока не содержит: `GameData/core` и замороженный корпус его не имеют.

Production package игры — `GameData/core`. Он staged в packaged build через `RuntimeDependencies` в `GV2.Build.cs` наравне со `Scripts/` и `Resources/`. UE host не читает test fixtures.

### `gv2-content`

```text
gv2-content validate <package-root> [--watch] [--poll-interval=MS] [--max-iterations=N] [--format=text|json]
gv2-content inspect  <package-root> <definition-id> [--provenance] [--format=text|json]
gv2-content describe <package-root> <definition-type> [--format=text|json]
gv2-content new      <package-root> <definition-type> <definition-id> [--format=text|json]
gv2-content refs     <package-root> <definition-id> [--format=text|json]
gv2-content rename   <package-root> <old-id> <new-id> [--format=text|json]
gv2-content index    <package-root> [--format=text|json]
gv2-content hash     <package-root> [--format=text|json]
gv2-content coverage <package-root> [--locale=LOCALE] [--format=text|json]
```

- `validate` — полная валидация пакета (schema, envelope, references, semantic constraints); выводит `content_hash` или список диагностик с JSON Pointer и спанами. Флаг `--watch` запускает live loop непрерывного наблюдения за изменениями файлов пакета (`--poll-interval=MS`, `--max-iterations=N`); каждая итерация изолирована, не публикует репозиторий, не накапливает состояние и не прерывается из-за транзиентных синтаксических ошибок в редактируемых файлах; выход по сигналам `SIGINT`/`SIGTERM` корректен.
- `inspect` — детальный просмотр одного definition по Stable ID; флаг `--provenance` добавляет source coordinates.
- `describe` — динамический справочник полей схемы для указанного `definition_type`. Выводит имена полей, типы (`int64`, `double`, `boolean`, `string`, `enum`, `array`, `map`, `object`, `union`, `ref`, `text_id`, `resource_ref`), обязательность (`required`/`optional`), ограничения (`min`, `max`, `values`, `min_items`, `unique`), ожидаемый target kind для ссылок и resource class для ресурсов, а также зарегистрированные semantic validators и extension schemas. Вывод формируется напрямую из схемы пакета.
- `new` — генерация валидной заготовки сущности с ID `<definition-id>` и типом `<definition-type>`. Проверяет синтаксис ID и соответствие `kind == <definition-type>`, гарантирует отсутствие дубликата ID в пакете. Заполняет обязательные поля типизированными плейсхолдерами с учётом ограничений схемы и опускает необязательные поля. Если файл нужного типа уже существует в `definitions/`, запись добавляется в существующий массив; если отсутствует — создаётся новый файл с валидным конвертом.
- `refs` — поиск всех входящих ссылок на указанный Stable ID (включая ссылки из `data`, `extensions` и редиректов). Выводит относительный путь к файлу, номер строки, колонку, JSON Pointer и ID ссылающегося определения. Если ссылок нет, выводит 0 ссылок и завершается с кодом 0.
- `rename` — атомарное переименование определения и всех ссылок на него на этапе разработки (Pre-publication, [ADR-0023](../ADR/0023-stable-id-publication-freeze.md)). Проверяет валидность обоих ID, совпадение `kind`, наличие исходного определения и отсутствие дубликата нового ID. Точечно переписывает строковые токены через AST-парсер без разрушения комментариев и форматирования соседних записей. Если пакет помечен как замороженный (`frozen: true` или `published: true` в `package.json5`), команда отказывается выполнять переименование на месте, требуя объявления `redirects`.
- `index` — выгрузка полного индекса Stable ID пакета в каноническом порядке. Группирует активные идентификаторы по `kind` (`actor`, `item`, `location`, `resource`, `screen`, `text`), а также отдельно выводит списки `redirects` (`source_id -> target_id`) и `tombstones`. Служит единым источником автодополнения для редакторов кода.
- `hash` — вычисление и вывод канонического `content_hash` пакета.
- `coverage` — сопоставление всех активных `text_id` определений типа `text` с ключами каталогов локализации `<package-root>/localization/<locale>.po` ([ADR-0022](../ADR/0022-external-translation-catalog.md)). Классифицирует ключи по четырём категориям: `translated` (непустой перевод), `empty` (пустая строка `msgstr ""`), `missing` (отсутствует в PO) и `extra` (устаревший ключ в PO без определения в репозитории). Является исключительно информационным отчётом (informational step), всегда возвращает exit code 0 и не влияет на валидность контента.

`--provenance` поддерживается только `inspect`; `--locale` — только `coverage`. Поддерживаются форматы вывода `--format=text` (по умолчанию) и `--format=json`. CLI не публикует repository и не запускает gameplay session. Absolute filesystem paths не попадают в его output.

### Интеграция с редакторами и Live Loop

Конфигурация редактора (`.vscode/tasks.json`, `.vscode/settings.json`, `Tools/Editor/`) предоставляет авторам контента бесшовную среду:
- **Автоматическая подсветка ошибок (Problem Matcher)**: фоновая задача `GV2: Watch Content (GameData/core)` запускает `gv2-content validate <package-root> --watch` и транслирует ошибки схем и синтаксиса в панель **Problems** редактора на конкретные строки файлов `.json5` сразу после сохранения.
- **Подсказки и автодополнение**: скрипт `Tools/Editor/generate_vscode_snippets.py` запрашивает `gv2-content index <package-root> --format=json` и генерирует `.vscode/gv2-content.code-snippets` для автодополнения Stable ID в редакторе.
- **Опциональность**: конфигурация редактора полностью изолирована и не является обязательной для сборки проекта; отсутствие файлов в `.vscode/` ничего не ломает. Подробная документация и regex для других IDE собраны в `Tools/Editor/README.md`.

### `gv2-headless`

```text
gv2-headless [--self-test] [--check-scripts] [--commands=N] [--seed=N] [--manifest=PATH] [--content-root=PATH] [--output-manifest=PATH] [--output-digest=PATH]
```

`--content-root` по умолчанию разрешается в `GameData/core`. Repository строится и закрепляется до создания Lua VM. Вывод в stdout содержит единую JSON-строку с метаданными прогона, `repository_content_hash`, `state_hash`, `digest_hash` и вложенным объектом `digest` (включающим `state_hash`). Опция `--manifest` воспроизводит записанную последовательность команд; при несовпадении `repository_content_hash` прогон завершается с exit code 2 до bootstrap. Опции `--output-manifest` и `--output-digest` сохраняют полный сериализованный `FRunManifest` и `FRunDigest` в указанные файлы.

Флаг `--check-scripts` запускает изолированную проверку дерева `Scripts/` без старта геймплея и без диспетчеризации команд. Он загружает манифест модулей, проверяет покрытие файлов, топологически разрешает граф зависимостей (проверяя отсутствие циклов, отсутствующих или незаявленных модулей) и компилирует каждый модуль с валидацией экспортной таблицы. При успехе выводит JSON `{"ok":true,"status":"ok","modules_checked":N,...}` и возвращает 0. При ошибке возвращает exit code 1 с выводом `module_id`, относительного пути и позиции ошибки.

## Exit codes

Exit code является machine-readable результатом; human-readable message не разбирается тестами.

| Код | `gv2-content` | `gv2-headless` |
|---|---|---|
| 0 | Success | Success |
| 1 | Invalid content | Runtime/Lua failure, ошибка скриптов при `--check-scripts` |
| 2 | Tool/configuration failure | Configuration failure: manifest `repository_content_hash` не совпадает с pinned snapshot |
| 5 | — | Replay failure: typed fault внутри `ReplayRunManifest`, включая `lua_release_mismatch` |
| 9 | — | Repository missing or invalid |
| 64 | — | Invalid argument, нечитаемый или неразбираемый manifest |
| 66 | — | Lua module tree not found |

Exit code одинаков для `--format=text` и `--format=json`.

## Shared fixtures and conformance

`Tests/Fixtures/PortableContentCore` — общий corpus для всех трёх host-ов: `valid/core`, `valid/test_mod` и именованные `invalid/*` случаи. `fixtures.index` содержит bytewise-sorted inventory; `Tools/Content/validate_pcc_fixtures.py` проверяет соответствие дерева этому inventory (CTest `pcc_shared_fixture_contract`) и, отдельным правилом (PCC-01), запрещает любой файл `expected*` внутри самого дерева — pinned-значения корпуса живут снаружи, рядом с ним.

**Заморожен (TAS-06, план [TestArchitectureAndLuaSpecs](../Plans/Archive/TestArchitectureAndLuaSpecs/README.md)).** `valid/core`/`valid/test_mod` больше не зеркалируют `GameData/core`: рост игрового контента их не трогает; изменение допустимо только когда предметом изменения являются сами правила разрешения контента (parsing/schema/override/redirect/tombstone/provenance), а не gameplay-сущности. Правило записано также в `Tests/Fixtures/PortableContentCore/README.md`, рядом с корпусом.

**Раздельные pinned-значения (TAS-07).** `Tests/Fixtures/expected_core_content_hash.txt` пинит хэш ТОЛЬКО замороженного тестового корпуса (`valid/core`); сверяется CTest `gv2_content_hash_core_fixture` и Unreal automation (`GV2.Runtime.ContentCore.CrossHostParity`). `GameData/core` не пинит content hash вовсе — CTest `gv2_content_validate_gamedata_core` остаётся smoke-проверкой (`gv2-content validate`, без сравнения хэша), поэтому рост игрового контента никогда не требует правки pinned-значения.

Conformance-наборы объявлены в portable headers (`Source/<Module>/Public/<Module>/Testing/`) и исполняются обоими host-ами из одного источника; отдельные копии positive/negative cases запрещены.

### Формат Lua-спеки (`Tests/Lua/`)

Правило, целиком выраженное в Lua, проверяется Lua-спекой, а не новым C++ conformance entry point (план [TestArchitectureAndLuaSpecs](../Plans/Archive/TestArchitectureAndLuaSpecs/README.md), [ADR-0024](../ADR/0024-lua-spec-runner.md)). Обнаружение и исполнение спек — generic portable runner (TAS-02); этот раздел фиксирует только формат, которому обязан следовать файл спеки, чтобы runner мог его исполнить.

Обнаружение (`GV2ContentHostSupport::DiscoverLuaSpecFiles()`), исполнение (`FRuntimeSession::RunLuaSpec()`) и деривация идентификатора провала (`GV2ContentHostSupport::LuaSpecIdentity` — `DeriveLuaSpecId()`, `MakeLuaSpecCaseFailure()`, `MakeLuaSpecFault()`, TAS-03) связаны в один вызов модулем `GV2TestSupport` (CMake target `gv2_test_support`, UBT module `GV2TestSupport`; зависит от `gv2_content_host_support` и `gv2_runtime_core` — единственный слой, зависящий от обоих, поскольку они сиблинги друг друга не видят). `GV2TestSupport::RunLuaSpecs(SpecRoot, Session, OutResult)` обнаруживает `*.lua` под `SpecRoot` на уже запущенной `Session` и агрегирует провалы (TAS-04). Test-only — ни один gameplay host его не линкует.

**Под-дерево `Tests/Lua/` определяет, какую сессию runner получает** — это не деталь реализации, а решение, которое принимает автор новой категории спек. Сейчас есть два под-дерева:

- `Tests/Lua/world/` — исполняется на уже стартовавшей **production**-сессии (реальный `GameData/core`, реальный `Scripts/bootstrap/manifest.lua`). Подходит правилам, которым достаточно того, что уже загружено в проде (TAS-12).
- `Tests/Lua/commands/` — исполняется на изолированной **fixture**-сессии (`GV2TestSupport::StartCommandValidatorFixtureSession()`; реальные `Scripts/runtime/{mutation_window,stable_id,validator_registry,command_dispatcher}.lua` + test-only `Tests/Fixtures/CommandValidatorSpecs/{manifest,driver}.lua`). Обязательна для правил, которым нужны тест-scoped фикстуры, зарегистрированные до заморозки глобального состояния (например, `game.commands.validators` — уже заморожен и пуст к моменту исполнения любой спеки на production-сессии) (TAS-13).

Оба host-а вызывают `RunLuaSpecs` дважды — по разу на каждое под-дерево с соответствующей сессией — а не одним рекурсивным сканом всего `Tests/Lua/`: `gv2-headless --self-test` — внутри self-test блока, после C++ conformance-проверок и до диспетчеризации digest-влияющих команд (exit code 16 при провале, `lua_spec_failed id=... code=... message=...` в `stderr` на каждый провал); Unreal automation — двумя тестами, `GV2.Runtime.Lua.SpecRunnerHost` (`world/`) и `GV2.Runtime.Lua.CommandValidatorSpecRunnerHost` (`commands/`), каждый представляет всё своё под-дерево, а не отдельные спеки. Отсутствующее под-дерево не является ошибкой ни для одного из host-ов. Добавление новой спеки в СУЩЕСТВУЮЩЕЕ под-дерево расширяет покрытие обоих host-ов без единой строки C++; добавление НОВОЙ категории, которой нужна ещё не существующая сессия, требует ровно одного вызова `RunLuaSpecs` на каждый host — не обёртки на спеку.

- Спека — `.lua`-файл где-то под `Tests/Lua/` (вложенность каталогов не ограничена). Спеки не входят в module tree `Scripts/`, не грузятся module loader-ом и не попадают в упакованную игру.
- Спека возвращает единственное значение — таблицу именованных кейсов:

  ```lua
  return {
      wrapper_rejects_repeated_access = function()
          -- Кейс целиком самодостаточен: сам строит нужное окружение
          -- (repository, session, state) и сам его использует.
          local world = require("core:module.runtime.world")
          local w1, w2 = game.instances.world(), game.instances.world()
          assert(w1 ~= w2, "repeated access must not return the same wrapper table")
      end,

      wrapper_rejects_wrong_kind = function()
          -- Независим от кейса выше: не читает и не изменяет ничего, что
          -- тот кейс мог оставить после себя.
          ...
      end,
  }
  ```

- Ключ таблицы — имя кейса, `^[a-z][a-z0-9_]*$` (тот же segment grammar, что у `module_id`/`instance_id`). Значение обязано быть функцией без аргументов.
- Кейс сигнализирует провал ошибкой Lua (`error(...)` либо неудавшийся `assert(...)`) и успех — обычным возвратом; возвращаемое значение, если оно есть, runner игнорирует.
- Кейс не зависит от других кейсов и от порядка исполнения: он не читает состояние, оставленное соседним кейсом, и не полагается на побочные эффекты `require`, вызванного другим кейсом. Любую фикстуру — repository, session, state — кейс строит сам внутри своей функции.
- Top-level код файла (вне тел кейсов) может декларировать `local` requires и чистые константы, но не заводит изменяемое состояние, разделяемое между кейсами: контракт спеки не гарантирует, исполнит ли runner тело файла один раз на все кейсы или заново на каждый кейс — это деталь стратегии исполнения runner-а (TAS-02), а не часть формата. Спека обязана корректно работать при любой из двух стратегий.
- Пустая таблица (`return {}`) — невалидная спека; runner обязан отклонить её как ошибку конфигурации, а не как ноль пройденных кейсов (TAS-02).
- Полный идентификатор провалившегося кейса (`<spec>.<case>`) и его точная деривация из пути файла и имени кейса — предмет TAS-03, не этого формата.

### Lua-спеки

Правило, выраженное в Lua, проверяется спекой в `Tests/Lua/` (ADR-0024). Спека — Lua-файл, возвращающий таблицу именованных кейсов:

```lua
return {
    case_name = function()
        assert(condition, "message")
    end,
}
```

Кейс независим и не зависит от порядка исполнения. Провал даёт стабильный идентификатор `<spec>.<case>` — например `world.domain_object.repeated_access_returns_distinct_wrappers`, — одинаковый в обоих host-ах.

Обнаружение рекурсивно по под-дереву и отсортировано. Под-дерево определяет сессию: `Tests/Lua/world/` исполняется на продакшн-сессии, `Tests/Lua/commands/` — на изолированной fixture-сессии. `Tests/Lua/` не входит в `Scripts/`, не грузится module loader-ом и не попадает в упакованную игру.

Запуск: `gv2-headless --self-test` (exit code 16 при провале спеки) и Unreal automation `GV2.Runtime.Lua.SpecRunnerHost` / `GV2.Runtime.Lua.CommandValidatorSpecRunnerHost` — один тест на под-дерево.

### Каноническая форма Conformance Entry Point

C++ entry point остаётся формой проверки C++ API. Все такие entry points используют единую сигнатуру и контракт возврата:

```cpp
GV2_PORTABLE_API std::string Run<Area>Conformance();
```

- **Семантика возврата**:
  - `""` (пустая строка) — все проверки набора успешно пройдены.
  - Непустая строка — стабильный идентификатор провалившегося case (в формате `<area>.<case_id>`, например `value_model.type_mismatch_throws`, `parse_limits.depth_ceiling_rejected`).
- **Инварианты**:
  - Идентификатор детерминирован и не содержит host-специфичных путей, указателей памяти, таймингов или localized текста.
  - При ошибке `gv2-headless --self-test` и Unreal Automation получают и сообщают идентичный case identifier.
  - UE Automation Tests оформляются как тонкие обёртки:
    ```cpp
    const std::string Error = GV2ContentCore::Testing::RunValueModelConformance();
    TestTrue(FString::Printf(TEXT("ValueModel conformance: %s"), UTF8_TO_TCHAR(Error.c_str())), Error.empty());
    ```
  - Headless `--self-test` использует единый формат вывода ошибки в `std::cerr`:
    ```cpp
    if (const std::string Error = RunValueModelConformance(); !Error.empty())
    {
        std::cerr << "pcc_value_model_conformance_failed: " << Error << "\n";
        return 1;
    }
    ```

| Entry point | Проверяет |
|---|---|
| `RunValueModelConformance()` | Value model, UTF-8 и copy/move семантика |
| `RunDiagnosticModelConformance()` | Context fields, nullopt и детерминированная сортировка диагностик |
| `RunBuildResultConformance()` | Success/Failure states, immutability и сортировка диагностик сборки |
| `RunPackageDescriptorConformance()` | Валидация дескрипторов пакетов, bindings и retirement rules |
| `RunParseLimitsConformance()` | UTF-8, BOM, лимиты вложенности, размера и контейнеров |
| `RunJson5LexerConformance()` | Лексический анализ JSON5, escape-последовательности, UTF-16 surrogates |
| `RunJson5ParserConformance()` | Синтаксический анализ JSON5 AST, детерминизм, нормализация чисел и дубликаты ключей |
| `RunSchemaRegistryConformance()` | Регистрация схем, поиск по версии, отказ дубликатов и semantic validators |
| `RunScalarValidationConformance()` | Скалярные поля (int64, number, bool, string, enum) и constraints |
| `RunContainerValidationConformance()` | Composite fields, discriminated unions, уникальность элементов массивов |
| `RunPresenceDefaultConformance()` | Required/optional поля, дефолты и deep copy изоляция |
| `RunSpecialFieldValidationConformance()` | Спецполя ref (target kind), text_id и resource_ref |
| `RunDefinitionEnvelopeConformance()` | Envelope файлов определений, согласование kind и дубликаты ID |
| `RunExtensionSchemaConformance()` | Extension schemas, extension site/namespace, defaults и cross-references |
| `RunStableIdConformance()` | Stable ID grammar |
| `RunJson5FixtureConformance()` | JSON5 parsing и diagnostics |
| `MakeRepresentativeCorePackageDescriptor()` | In-memory repository corpus |
| `RunLuaMarshallerConformance()` | Value marshalling обоих portable типов |
| `RunLuaRepositoryAccessConformance()` | `game.repository` семантика |
| `RunRunManifestConformance()` | `FRunManifest` serialization, round-trip deserialization, hash validation, determinism |
| `RunRunDigestConformance()` | `FRunDigest` canonical SHA-256 computation, sensitivity to manifest/result/state_hash, determinism, round-trip |
| `RunRunReplayConformance()` | `ReplayRunManifest` выполнение команд, отказ при несовпадении Lua release или repository hash |
| `RunLuaSpecRunnerConformance()` | Механизм `FRuntimeSession::RunLuaSpec` (TAS-02): пропуск непровалившихся кейсов, детерминированный порядок, `require()` уже загруженного модуля, отказ на невалидном формате спеки. Проверяет C++-механизм, не Lua-правило (ADR-0024) |

## Integration gate

`.github/workflows/linux-ci.yml` обязателен и состоит из трёх независимых jobs:

| Job | Runner | Содержание |
|---|---|---|
| CMake, CTest and headless | hosted Ubuntu | Configure/build, CTest (включая `host_conformance_parity_contract`), явные `gv2-headless --self-test`, `gv2-headless --check-scripts` и `gv2-content` smoke commands (включая информационный `gv2-content coverage`) |
| Documentation contracts | hosted Ubuntu | `Tools/Documentation/validate_docs.py`: UTF-8, front matter, relative links/anchors, targets `depends_on`/`decisions`, отсутствие cycles |
| Unreal `GV2.Runtime` | self-hosted linux x64 | Build `GV2Editor` и полный automation filter `GV2.Runtime` |

`Tools/Content/validate_host_conformance_parity.py` (CTest `host_conformance_parity_contract`) проверяет:
1. Отсутствие host-локальных self-тестов в `Headless/Source/main.cpp`.
2. Вызов всех переносимых entry points из `Source/*/Public/*/Testing/*.h` обоими host-ами (Headless и UE Automation).
3. **Отсутствие встроенного Lua в production C++**: любые строковые литералы с gameplay/validation кодом или `R"lua(...)` сырые литералы в production C++ файлах (`Source/`, исключая conformance/тестовые фикстуры) ломают CI. Вся gameplay-логика обязана находиться в `Scripts/`.
4. **Запрет нового C++ conformance entry point на Lua-правило** (TAS-14, ADR-0024): любой `*Conformance.cpp`, встраивающий Lua-исходник, обязан быть явно перечислен в одном из двух списков внутри скрипта — `LEGACY_LUA_RULE_CONFORMANCE_FILES` (унаследованные до ADR-0024 наборы, дальше не расширяется — миграция на спеки при следующем touch) или `MECHANISM_LUA_FIXTURE_CONFORMANCE_FILES` (тестирует C++-механизм — сам spec runner, `ReplayRunManifest` — синтетическими фикстурами без прообраза в `Scripts/`, не gameplay-правило). Файл вне обоих списков со встроенным Lua ломает CI с указанием, какой список пополнить и почему.

Унаследованные до ADR-0024 наборы, ещё проверяющие Lua-правила из C++:

| Набор | Строк | Предмет |
|---|---|---|
| `GV2LuaRepositoryConformance.cpp` | 489 | Семантика `game.repository` |
| `GV2ValidatorRegistryConformance.cpp` | 332 | Реестр валидаторов и порядок |

Список закрыт и не расширяется. Каждый набор мигрирует в спеки при следующем изменении его предмета.

Unreal runner обязан иметь UE 5.8 в `${UE_ROOT}` (default `/opt/unreal-engine`, переопределяется repository variable). Fork pull request не запускается на self-hosted runner. Нулевой exit code Unreal process недостаточен: job обязан найти marker `TEST COMPLETE. EXIT CODE: 0` и отклонить любой `Result={Fail}` в automation log.

## Локальные эквиваленты

```bash
cmake -S . -B cmake-build-ci -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-ci --parallel 2
ctest --test-dir cmake-build-ci --output-on-failure
./cmake-build-ci/Headless/gv2-headless --self-test
./cmake-build-ci/Headless/gv2-headless --check-scripts
./cmake-build-ci/Tools/Content/gv2-content validate GameData/core
./cmake-build-ci/Tools/Content/gv2-content coverage GameData/core
python3 Tools/Documentation/validate_docs.py
```

## Verification

- Portable targets собираются без Unreal headers и без установленного Unreal Engine.
- Один corpus даёт одинаковый `content_hash` в `gv2-content`, `gv2-headless` и Unreal automation.
- Каждый негативный CTest проверяет и stable diagnostic code, и exit code.
- `GameData/core` валидируется CI наравне с fixtures.
