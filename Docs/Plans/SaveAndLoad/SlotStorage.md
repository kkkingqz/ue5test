---
title: Slot Storage Tasks
status: draft
version: 1.1
updated: 2026-08-15
depends_on:
  - README.md
  - ../../Architecture/BuildAndTooling.md
decisions:
  - ../../ADR/0020-cpp-scope-criterion.md
  - ../../ADR/0021-opaque-save-container.md
---

# M2 — Slot Storage

> **Материализует:** [Build and Tooling](../../Architecture/BuildAndTooling.md).
> **Задачи:** SAV-05…07.
> **Результат:** host отдаёт и принимает непрозрачные байты по `save_slot_id`.

## Результат этапа

Host умеет отдавать и принимать непрозрачные байты по `save_slot_id`, не зная их формата. Это единственный C++ в плане.

## Задачи

- [x] **SAV-05 — Определить portable-интерфейс примитива**
  - Интерфейс объявляется в `GV2RuntimeCore` по образцу существующих host-service интерфейсов и предоставляется session-у композиционным корнем.
  - Операции: чтение слота, запись слота; результаты типизированы (`ok`, `not_found`, `unreadable`, `failure`).
  - Done: интерфейс не содержит путей, `FString`, UObject и filesystem-типов; Lua получает байты и typed result, но не путь; отсутствие реализации примитива не мешает старту session без сохранения.
  - Evidence: `GV2RuntimeCore::ISaveSlotStorage` (`Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2HostServices.h`) объявлен рядом с существующими `IResourceCatalog`/`ILocalizationAdapter`: `ReadSlot(const std::string& SlotId) const` и `WriteSlot(const std::string& SlotId, const std::string& Bytes)`, результат — `ESaveSlotResult{Ok, NotFound, Unreadable, Failure}` в `FSaveSlotReadResult`/`FSaveSlotWriteResult`. Ни один тип интерфейса не ссылается на путь, `FString`, UObject или filesystem — `Bytes` это `std::string` произвольных байт (может содержать NUL). Интерфейс не является параметром `FRuntimeSession::Start` (сигнатура не менялась), поэтому отсутствие реализации структурно не может помешать старту сессии без сохранения — то же свойство, что уже верно для `IResourceCatalog`/`ILocalizationAdapter`. Портовый helper `IsValidSaveSlotId` фиксирует грамматику допустимого `save_slot_id` (`^[a-z][a-z0-9_]*$`, тот же segment grammar, что у `module_id`/имён Lua-спек-кейсов) — единая точка правды, которую использует реализация SAV-06.

- [x] **SAV-06 — Реализовать примитив в обоих host-ах**
  - Зависимости: SAV-05.
  - `save_slot_id` разрешается в физический путь только внутри реализации; любая другая адресация запрещена.
  - Done: запись идёт во временный файл с атомарной подменой слота; предыдущая копия сохраняется; неудачная запись оставляет предыдущий слот валидным и читаемым; попытка адресовать слот вне разрешённого корня отклоняется; поведение UE и headless совпадает.
  - Evidence: `GV2RuntimeCore::FFilesystemSaveSlotStorage` (`Source/GV2RuntimeCore/Private/GV2SaveSlotStorage.cpp`) — одна реализация, используемая буквально обоими host-ами без дублирования (`gv2_runtime_core`/`GV2RuntimeCore` уже линкуется и headless, и UE): `std::filesystem::path` уже принят как portable-тип на этом уровне (те же discovery-заголовки `GV2ContentHostSupport`), поэтому «поведение UE и headless совпадает» доказано буквальным совпадением кода, а не отдельной проверкой параллельных реализаций. Каждый host передаёт свой корневой каталог конструктору. `ResolveSlotPath` отклоняет `save_slot_id`, не прошедший `IsValidSaveSlotId`, до любого обращения к файловой системе — адресация вне разрешённого корня структурно невозможна (слот всегда `RootDir / (SlotId + ".save")`, а `SlotId` не может содержать `/` или `..`). Запись пишет в `<slot>.save.tmp` в том же каталоге, затем публикует одним `std::filesystem::rename` в `<slot>.save` — атомарным на одном volume; любой отказ до `rename` (не удалось открыть/записать temp-файл, `rename` вернул `error_code`) оставляет уже опубликованный `<slot>.save` нетронутым и возвращает `Failure`, temp-файл подчищается.

- [x] **SAV-07 — Покрыть примитив conformance**
  - Зависимости: SAV-06.
  - Примитив является C++ API, поэтому проверяется C++ entry point, а не спекой.
  - Done: покрыты запись и чтение roundtrip, чтение отсутствующего слота, чтение повреждённого слота, прерванная запись с сохранением предыдущего содержимого; набор исполняется обоими host-ами; `BuildAndTooling` описывает примитив и его размещение.
  - Evidence: `GV2RuntimeCore::Testing::RunSaveSlotStorageConformance()` (`Source/GV2RuntimeCore/Public/GV2RuntimeCore/Testing/GV2SaveSlotStorageConformance.h` + `Private/GV2SaveSlotStorageConformance.cpp`) — нулевой arity, сам создаёт и удаляет временный каталог (`FScopedTempDir`), как остальные наборы в `GV2RuntimeCore::Testing`. Кейсы: write/read roundtrip с произвольными байтами включая embedded NUL; повторная запись атомарно замещает предыдущую (без наблюдаемой смеси старого/нового содержимого); чтение никогда не записанного слота → `NotFound` с пустыми байтами; слот, чей путь занят каталогом вместо файла → `Unreadable`, не `NotFound` и не падение; прерванная запись (temp-путь предварительно занят каталогом, поэтому открыть его для записи невозможно) не публикует новое содержимое и оставляет предыдущий baseline-слот читаемым без изменений; `save_slot_id` вне грамматики (`../escape`, `a/b`, пустая строка, `Slot` с заглавной буквой) отклоняется и на запись, и на чтение, а путь-эскейп за пределы `RootDir` не создаётся. Подключено к обоим host-ам: `gv2-headless --self-test` (после `RunLuaSpecRunnerConformance`, exit code 17 при провале, `save_slot_storage_conformance_failed case=...` в stderr) и `GV2.Runtime.SaveAndLoad.SaveSlotStorageConformance` (`Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`, тот же паттерн, что `ValidatorRegistryConformanceCrossHost`/`SpecRunnerConformanceCrossHost`).
    - Проверено: `ctest` 57/57; `gv2-headless --self-test` — `state_hash` не изменился (`2f17eb28ab16acb4f5cfbeaf49cc3ea302a09398f4980d9e9071c1a21e987773`), новый conformance-кейс проходит; UE `GV2.Runtime` — 52/52 (был 51/51 до добавления `SaveSlotStorageConformance`, регрессий нет), включая новый `GV2.Runtime.SaveAndLoad.SaveSlotStorageConformance` отдельным прогоном.
    - `Docs/Architecture/BuildAndTooling.md` получил раздел «Save slot storage primitive» и запись примитива в таблицу Portable targets.

## Проверка milestone

- [x] Host не интерпретирует содержимое слота.
- [x] Lua не получает путей и не выполняет filesystem I/O.
- [x] Неудачная запись не разрушает предыдущий слот.
- [x] UE и headless ведут себя одинаково.
