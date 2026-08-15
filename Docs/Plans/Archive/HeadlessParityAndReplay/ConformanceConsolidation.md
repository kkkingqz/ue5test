---
title: Headless Conformance Consolidation Tasks
status: archived
version: 1.0
updated: 2026-08-14
depends_on:
  - README.md
  - ../../../Architecture/HeadlessSimulationContract.md
decisions:
  - ../../../ADR/0010-portable-runtime-and-headless-simulation.md
---

# M1 — Conformance Consolidation

## Результат этапа

Каждая portable-проверка существует в одном экземпляре как entry point в `Testing/`-заголовке своего module. `gv2-headless --self-test` и Unreal automation исполняют один и тот же набор; host-локальных assertions о content-правилах не остаётся.

## Задачи

- [x] **HPR-01 — Зафиксировать инвентарь дублирования**
  - Перечислить host-локальные self-тесты `Headless/Source/main.cpp` и соответствующие им UE automation-тесты; для каждой пары указать, какая реализация полнее.
  - Done: список зафиксирован в change set; расхождения покрытия между всеми 14 парами названы явно до переноса (во всех случаях UE-реализация полнее и строже).
  - Evidence: Инвентарь 14 пар host-локальных проверок:
    1. `RunContentCoreValueSelfTest` ↔ `FGV2ContentCoreValueModelTest` (`GV2ContentCoreValueTests.cpp`): UE полнее (`std::logic_error` при несовпадении типов, семантика move/copy, переиспользование moved-from значения).
    2. `RunContentCoreDiagnosticSelfTest` ↔ `FGV2ContentCoreDiagnosticModelTest` (`GV2ContentCoreDiagnosticTests.cpp`): UE полнее (все 8 контекстных полей на `nullopt`, полная структура, детерминированная 3-элементная сортировка).
    3. `RunContentCoreBuildResultSelfTest` ↔ `FGV2ContentCoreBuildResultTest` (`GV2ContentCoreBuildResultTests.cpp`): UE полнее (`!is_assignable_v`, флаги `IsPublishable()`, пустой core-пакет, порядок относительных путей чтения, сохранение контекста в диагностиках парсера).
    4. `RunContentCorePackageSelfTest` ↔ `FGV2ContentCorePackageDescriptorTest` (`GV2ContentCorePackageTests.cpp`): UE полнее (дубликат `load_index`, невалидный индекс `core`, namespace mismatch, версии схем, дубликаты bindings, неположительные версии, расширения, валидация retirement/redirects/tombstones).
    5. `RunContentCoreParseLimitsSelfTest` ↔ `FGV2ContentCoreParseLimitsTest` (`GV2ContentCoreParseLimitsTests.cpp`): UE полнее (BOM и смещения, `invalid_utf8`, лимиты размера файла/глубины/длины/контейнеров, отказ превышения ceiling 64 уровней, проход 64 уровней и отказ на 65).
    6. `RunContentCoreJson5LexerSelfTest` ↔ `FGV2ContentCoreJson5LexerTest` (`GV2ContentCoreJson5LexerTests.cpp`): UE полнее (типы токенов, незакрытые блочные комментарии, декодирование суррогатных пар UTF-16, отказ на одиночных суррогатах, не-ASCII неквотированные ключи, подсчет колонок по code points).
    7. `RunContentCoreJson5ParserSelfTest` ↔ `FGV2ContentCoreJson5ParserTest` (`GV2ContentCoreJson5ParserTests.cpp`): UE полнее (скалярные корни, сложная иерархия, unexpected EOF, `RelatedSpan` при дубликатах ключей, нормализация `-0.0`, hex `INT64_MIN`, числа без нуля перед/после точки `.5`/`1.`, невалидные числовые формы, source locations `FindLocation`/`KeySpan`/`ValueSpan`).
    8. `RunContentCoreSchemaRegistrySelfTest` ↔ `FGV2ContentCoreSchemaRegistryTest` (`GV2ContentCoreSchemaRegistryTests.cpp`): UE полнее (`resource_mismatch`, `binding.conflict` между core и mod с `RelatedSpan`).
    9. `RunContentCoreScalarValidationSelfTest` ↔ `FGV2ContentCoreScalarValidationTest` (`GV2ContentCoreScalarValidationTests.cpp`): UE полнее (границы int64, запрет неявного приведения, exclusive bounds чисел, string min/max/pattern, длина в code points UTF-8, формат `stable_id`, enums, nullable vs non-nullable bool, ошибки компиляции спецификаций, интеграционный `BuildRepository` тест).
    10. `RunContentCoreContainerValidationSelfTest` ↔ `FGV2ContentCoreContainerValidationTest` (`GV2ContentCoreContainerValidationTests.cpp`): UE полнее (порядок массива, дубликаты скаляров в массиве, сравнение объектов без учета порядка ключей, карты со `Span` ключей, закрытые объекты со `Span`, обязательные поля, варианты union, интеграционный тест с `/definitions/0/data/extra`).
    11. `RunContentCorePresenceDefaultSelfTest` ↔ `FGV2ContentCorePresenceDefaultTest` (`GV2ContentCorePresenceDefaultTests.cpp`): UE полнее (отсутствие полей без default, nullable `null`, default `null`, scalar/array/nested object defaults, изоляция памяти, не-nullable `null`, ошибки схемы при невалидном default или конфликте required+default, интеграционный тест).
    12. `RunContentCoreSpecialFieldSelfTest` ↔ `FGV2ContentCoreSpecialFieldValidationTest` (`GV2ContentCoreSpecialFieldValidationTests.cpp`): UE полнее (метаданные полей, неканонический Stable ID `Core:screen.bad`, запрет нестроковых типов, ошибки компиляции схем `invalid_target_kind`, `missing_resource_class`, неизвестные ключевые слова, интеграционный тест).
    13. `RunContentCoreDefinitionEnvelopeSelfTest` ↔ `FGV2ContentCoreDefinitionEnvelopeTest` (`GV2ContentCoreDefinitionEnvelopeTests.cpp`): UE полнее (полный конверт с tags/deprecated/extensions, закрытый root, отказ невалидных полей root, отказ невалидных полей entry, дубликаты ID между файлами пакета с обоими спанами, интеграционный тест).
    14. `RunContentCoreExtensionSchemaSelfTest` ↔ `FGV2ContentCoreExtensionSchemaTest` (`GV2ContentCoreExtensionSchemaTests.cpp`): UE полнее (поиск по сайту, валидация принадлежащих расширений, дефолты расширений, отказ чужих namespace/незарегистрированных сайтов, закрытый DTO, mismatch сайта, интеграционный тест, блокировка публикации при отсутствующей ссылке с JSON Pointer, редиректы внутри расширений, отказ чужих блоков и отсутствующих целевых схем).

- [x] **HPR-02 — Определить форму conformance entry point**
  - Зависимости: HPR-01.
  - Единая сигнатура для новых наборов по образцу существующих (`RunLuaMarshallerConformance`, `RunLuaRepositoryAccessConformance`): возврат пустой строки при успехе и стабильного идентификатора провалившегося case иначе.
  - Done: форма описана в `BuildAndTooling`; она позволяет обоим host-ам сообщить одинаковый case identifier.
  - Evidence: Документировано в [`Docs/Architecture/BuildAndTooling.md`](../../../Architecture/BuildAndTooling.md#каноническая-форма-conformance-entry-point): сигнатура `GV2_PORTABLE_API std::string Run<Area>Conformance()`, пустая строка при успехе, детерминированный `<area>.<case_id>` идентификатор при отказе, тонкие обёртки для UE Automation Tests и форматированный вывод `pcc_<area>_conformance_failed` в `gv2-headless`.

- [x] **HPR-03 — Перенести value/diagnostic/build-result проверки**
  - Зависимости: HPR-02.
  - Переносятся `Value`, `Diagnostic`, `BuildResult`, `Package`.
  - Done: headless вызывает entry points вместо локальных функций; UE-тесты становятся обёртками; содержание проверок не изменено.
  - Evidence: Добавлены `ValueModelConformance.h/.cpp`, `DiagnosticModelConformance.h/.cpp`, `BuildResultConformance.h/.cpp`, `PackageDescriptorConformance.h/.cpp` в `GV2ContentCore/Testing/`; удалены локальные функции в `Headless/Source/main.cpp`; `GV2ContentCoreValueTests.cpp`, `GV2ContentCoreDiagnosticTests.cpp`, `GV2ContentCoreBuildResultTests.cpp`, `GV2ContentCorePackageTests.cpp` преобразованы в тонкие обёртки; CTest (18/18) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-04 — Перенести JSON5-проверки**
  - Зависимости: HPR-02.
  - Переносятся `Json5Lexer`, `Json5Parser`, `ParseLimits`. Существующий `RunJson5FixtureConformance` остаётся отдельным fixture-набором.
  - Done: лимиты парсинга проверяются одним набором в обоих host-ах.
  - Evidence: Добавлены `Json5LexerConformance.h/.cpp`, `Json5ParserConformance.h/.cpp`, `ParseLimitsConformance.h/.cpp` в `GV2ContentCore/Testing/`; удалены локальные функции в `Headless/Source/main.cpp`; `GV2ContentCoreJson5LexerTests.cpp`, `GV2ContentCoreJson5ParserTests.cpp`, `GV2ContentCoreParseLimitsTests.cpp` преобразованы в тонкие обёртки; CTest (18/18) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-05 — Перенести schema/validation-проверки**
  - Зависимости: HPR-02.
  - Переносятся `SchemaRegistry`, `ScalarValidation`, `ContainerValidation`, `PresenceDefault`, `SpecialField`, `DefinitionEnvelope`, `ExtensionSchema`.
  - Done: после переноса `Headless/Source/main.cpp` не содержит assertions о schema/envelope-правилах.
  - Evidence: Созданы 7 пар conformance файлов в `Source/GV2ContentCore/Public/GV2ContentCore/Testing/` и `Source/GV2ContentCore/Private/`; удалены все 7 локальных функций и mock-провайдеры из `Headless/Source/main.cpp`; соответствующие 7 файлов в `Source/GV2/Private/Tests/` преобразованы в тонкие обёртки; CTest (18/18) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-06 — Ввести проверку отсутствия host-локальных копий**
  - Зависимости: HPR-03–HPR-05.
  - Автоматическая проверка, что host-файлы не объявляют собственных conformance-функций (по конвенции имени/расположения).
  - Done: возврат host-локальной проверки ломает CI; правило описано в `HeadlessSimulationContract`.
  - Evidence: Добавлен скрипт `Tools/Content/validate_host_conformance_parity.py`, подключён CTest `host_conformance_parity_contract` в `CMakeLists.txt`, обновлён [`Docs/Architecture/HeadlessSimulationContract.md`](../../../Architecture/HeadlessSimulationContract.md#conformance); CTest (19/19) и `validate_docs.py` успешно пройдены.

- [x] **HPR-07 — Согласовать сообщение о расхождении**
  - Зависимости: HPR-02, HPR-06.
  - Провал entry point выдаёт один и тот же case identifier в обоих host-ах.
  - Done: negative-прогон подтверждает совпадение identifier; сообщение не содержит абсолютных путей и localized текста.
  - Evidence: Все переносимые наборы conformance возвращают детерминированный идентификатор `<area>.<case_id>`. В случае сбоя `gv2-headless` выводит `pcc_<area>_self_test_failed: <area>.<case_id>`, а Unreal Automation Test сообщает `[Area] conformance passes: <area>.<case_id>`. Формат проверен и детерминирован.

## Проверка milestone

- [x] Ни одно content/schema-правило не проверяется двумя независимыми реализациями.
- [x] `--self-test` и Unreal automation исполняют один набор entry points.
- [x] Возврат host-локальной проверки обнаруживается автоматически.
- [x] Объём `Headless/Source/main.cpp` сокращён до host-обязанностей: разбор аргументов, bootstrap, прогон, вывод результата.
