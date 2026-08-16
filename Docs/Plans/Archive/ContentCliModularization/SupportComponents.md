---
title: Support Components Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ../../../Architecture/BuildAndTooling.md
decisions:
  - ../../../ADR/0018-portable-content-core-module.md
---

# M1 — Support Components

## Результат этапа

Общие механизмы форматирования вывода, работы с AST JSON5 и слежения за файловой системой выделены в переиспользуемые модули в `Tools/Content/Source/Support/`.

## Задачи

- [x] **CCM-01 — Выделение `CliOutput` и экранирования JSON**
  - Описание: Создать `Tools/Content/Source/Support/CliOutput.h` и `.cpp` с функциями `WriteJsonEscapedString`, `EmitDiagnosticsFailure` и общими перечислениями `EOutputFormat`, `EExitCode`.
  - Done: Общий модуль форматирования скомпилирован и используется.
  - Evidence: Созданы `Tools/Content/Source/Support/CliOutput.h` и `CliOutput.cpp`; `Tools/Content/CMakeLists.txt` и `main.cpp` обновлены; CTest (57/57 passed).

- [x] **CCM-02 — Выделение `Json5AstRewriter`**
  - Описание: Создать `Tools/Content/Source/Support/Json5AstRewriter.h` и `.cpp`, перенеся туда парсер токенов, структуры AST, функции сохранения пробелов/комментариев и переписывания строковых токенов для команд `new` и `rename`.
  - Done: AST-парсер изолирован и протестирован.
  - Evidence: Созданы `Tools/Content/Source/Support/Json5AstRewriter.h` и `Json5AstRewriter.cpp`; функции генерации заготовок, форматирования JSON5 и `ReplaceStringTokens` вынесены из `main.cpp`; CTest (57/57 passed).

- [x] **CCM-03 — Выделение `DirectoryWatcher`**
  - Описание: Создать `Tools/Content/Source/Support/DirectoryWatcher.h` и `.cpp` со структурами `FDirectorySnapshot`, `TakeDirectorySnapshot`, циклом опроса и обработкой системных сигналов для режима `validate --watch`.
  - Done: Модуль слежения за файлами изолирован.
  - Evidence: Созданы `Tools/Content/Source/Support/DirectoryWatcher.h` и `DirectoryWatcher.cpp`; `RunDirectoryWatchLoop` интегрирован в `RunValidate`; CTest (57/57 passed).

## Проверка milestone

- [x] Модули поддержки собраны в CMake target `gv2-content`.
- [x] Отсутствуют циклические зависимости между Support модулями.
