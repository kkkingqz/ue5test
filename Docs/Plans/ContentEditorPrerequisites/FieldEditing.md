---
title: Field Editing Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - README.md
  - ../../Architecture/BuildAndTooling.md
---

# M2 — Field Editing

> **Материализует:** [Build and Tooling](../../Architecture/BuildAndTooling.md) в части редактирования контента инструментами.
> **Задачи:** CEP-04…07.
> **Результат:** значение поля меняется без переписывания файла.

## Результат этапа

Редактор сможет сохранить изменённое поле, не уничтожив комментарии и форматирование остального файла.

## Задачи

- [x] **CEP-04 — Перевод позиции узла в смещение**
  - Зависимости: CEP-01.
  - Спан несёт `StartLine`/`StartColumn`/`EndLine`/`EndColumn`; байтовых смещений нет, а точечная замена требует именно их.
  - Done: перевод позиции в смещение выполняется по исходному тексту с учётом UTF-8 и BOM; перевод и обратная проверка покрыты тестами на многобайтовых символах и на строках с разными окончаниями; примитив доступен переписывателю.
  - Evidence: `SourcePositionToByteOffset` и `SourceSpanToByteRange` в `Tools/Content/Source/Support/Json5AstRewriter.h/.cpp`; `Json5AstRewriterConformance.cpp` (`TestPositionToOffsetAscii`, `TestPositionToOffsetMultibyteUtf8`, `TestPositionToOffsetLineEndings`, `TestPositionToOffsetBom`); CTest `gv2_content_self_test`.

- [x] **CEP-05 — Правка значения поля по JSON-указателю**
  - Зависимости: CEP-04.
  - Done: `SetFieldValue(content, json_pointer, value)` заменяет текст в границах узла и не трогает ничего вне них; комментарии, отступы, порядок ключей и висячие запятые остального файла сохраняются; несуществующий указатель, указатель на контейнер вместо значения и непереносимое значение дают раздельные типизированные отказы; round-trip проверяется на файле с комментариями до, внутри и после изменяемой записи.
  - Evidence: `SetFieldValue` в `Tools/Content/Source/Support/Json5AstRewriter.h/.cpp`; `Json5AstRewriterConformance.cpp` (`TestSetFieldValue`); CTest `gv2_content_self_test`; `test_authoring_tools.py` (раздел 38).

- [x] **CEP-06 — Удаление записи definition**
  - Зависимости: CEP-04.
  - Done: `RemoveDefinitionEntry(content, definition_id)` удаляет запись вместе с её запятой и не переформатирует соседние; удаление последней записи оставляет валидный пустой массив; удаление несуществующей записи — типизированный отказ; удаление записи, на которую есть ссылки, отклоняется с перечислением ссылающихся — по данным `refs`.
  - Evidence: `RemoveDefinitionEntry` в `Tools/Content/Source/Support/Json5AstRewriter.h/.cpp`; `Json5AstRewriterConformance.cpp` (`TestRemoveDefinitionEntry`); `DeleteCommand.cpp` (проверка `ScanValueForReferences` и отказ `referenced_by_definitions`); `test_authoring_tools.py` (раздел 39).

- [x] **CEP-07 — Поверхность CLI и синхронизация contract**
  - Зависимости: CEP-05, CEP-06.
  - Done: `gv2-content set <package-root> <definition-id> <json-pointer> <value>` и `gv2-content delete <package-root> <definition-id>` доступны с `--format=json` и стабильными exit codes наравне с прочими командами; обе исполняются в CTest на замороженном корпусе, а не на живом контенте; [Build and Tooling](../../Architecture/BuildAndTooling.md) описывает обе команды и правило сохранения форматирования.
  - Evidence: `SetCommand.h/.cpp`, `DeleteCommand.h/.cpp`, `main.cpp`, `CliOutput.cpp`; CTests `gv2_content_set_*`, `gv2_content_delete_*`, `gv2_content_authoring_tools_python`; `BuildAndTooling.md`.

## Проверка milestone

- [x] Смена значения поля меняет в diff одну строку.
- [x] Комментарии до, внутри и после записи сохраняются.
- [x] Удаление записи не трогает соседние.
- [x] Удаление записи с живыми ссылками отклоняется.
