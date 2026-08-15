---
title: Rename Support Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - SchemaDrivenAuthoring.md
  - ../../../Architecture/StableIDSpecification.md
---

# M3 — Rename Support

## Результат этапа

До заморозки идентификаторов переименование выполняется инструментом: автор видит все обратные ссылки и переписывает их одной командой, а не поиском по репозиторию.

## Задачи

- [x] **CAT-07 — Реализовать `gv2-content refs`**
  - `refs <package-root> <definition-id>` перечисляет, откуда на ID ссылаются: `ref`, `text_id`, `resource_ref`, redirects и tombstones.
  - Done: вывод содержит package-relative путь и позицию каждой ссылки; ссылки из `data` и из materialized extension blocks находятся одним путём; `--format=text|json`; отсутствие ссылок не является ошибкой.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunRefs`, `ScanValueForReferences`, `EscapeJsonPointerSegment`), `Tools/Content/test_authoring_tools.py` (тесты `refs` text, json, unreferenced, invalid ID, missing args), `Tools/Content/CMakeLists.txt` (`gv2_content_refs_text`, `gv2_content_refs_json`, `gv2_content_refs_unreferenced`, `gv2_content_refs_rejects_invalid_id`), CTest (43/43 passed).

- [x] **CAT-08 — Реализовать `gv2-content rename`**
  - `rename <package-root> <old-id> <new-id>` переписывает определение и все найденные ссылки.
  - Done: команда отказывается работать, если новый ID уже существует или не соответствует грамматике; изменения атомарны — либо переписаны все файлы, либо ни один; после переименования `validate` проходит; форматирование и комментарии соседних записей не разрушаются.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunRename`, `FFileRenameWork`, AST-based token replacement с `LexJson5`), `Tools/Content/test_authoring_tools.py` (тесты переименования с проверкой сохранения комментариев и ссылок, JSON/text форматы, валидация после переименования, отказы при несуществующем старом ID, дубликате нового ID, невалидной грамматике и несовпадении kind), `Tools/Content/CMakeLists.txt` (`gv2_content_rename_*`), CTest (51/51 passed).

- [x] **CAT-09 — Уточнить область инварианта неповторного использования**
  - Формулировка «опубликованный Stable ID не переиспользуется» сейчас безусловна в `Overview`, `StableIDSpecification` и `AGENTS.md`, но на этапе разработки идентификаторы не заморожены.
  - Done: инвариант уточнён до опубликованного в production контента; зафиксировано, чем именно определяется момент заморозки; если ревью сочтёт это ослаблением принятого решения, изменение оформляется ADR до правки контрактов.
  - Evidence: `Docs/ADR/0023-stable-id-publication-freeze.md` (принят ADR о разделении Pre-publication authoring phase и Post-publication freeze), `Docs/ADR/README.md`, `Docs/Architecture/StableIDSpecification.md` (раздел «Lifecycle and publication freeze», «Redirects and tombstones»).

- [x] **CAT-10 — Ограничить область применения инструмента**
  - Зависимости: CAT-08, CAT-09.
  - Done: `rename` неприменим к замороженному контенту и отказывается работать явным сообщением; `BuildAndTooling` описывает обе команды; CTest покрывает успешный путь, отказ по существующему ID и отказ по грамматике.
  - Evidence: `Tools/Content/Source/main.cpp` (проверка `frozen: true` / `published: true` в дескрипторе пакета с отказом `package_frozen`), `Docs/Architecture/BuildAndTooling.md` (документация `refs` и `rename`), `Tools/Content/test_authoring_tools.py` (тест 23 на отказ при замороженном пакете), `Tools/Content/CMakeLists.txt` (CTest `gv2_content_rename_*`), CTest (51/51 passed).

## Проверка milestone

- [x] Все обратные ссылки на ID находятся одной командой.
- [x] Переименование атомарно и оставляет корпус валидным.
- [x] Область действия инварианта зафиксирована в контрактах.
