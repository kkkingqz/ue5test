---
title: Coverage Tooling Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - HostResolution.md
  - ../../../Architecture/BuildAndTooling.md
---

# M4 — Coverage Tooling

## Результат этапа

Полнота перевода видна отчётом и не является условием валидности контента.

## Задачи

- [x] **LOC-08 — Реализовать отчёт о полноте**
  - Команда сопоставляет множество `text_id` репозитория с ключами каталога locale.
  - Done: отчёт различает отсутствующий перевод, лишний ключ и ключ с пустым значением; вывод machine-readable; команда не является частью `validate` и не влияет на его exit code.
  - Evidence: В `Tools/Content/Source/main.cpp` добавлена CLI-команда `gv2-content coverage <package-root> [--locale=LOCALE] [--format=text|json]` с machine-readable JSON/text выводом (состояния: `translated_keys`, `empty_keys`, `missing_keys`, `extra_keys`, процент покрытия); в `Tools/Content/test_authoring_tools.py` добавлен тест 34, подтверждающий корректность отчёта и неизменность exit code `gv2-content validate` при неполных переводах; CTest `gv2_content_authoring_tools_python` и все 57 тестов зелёные.

- [x] **LOC-09 — Определить участие в CI**
  - Done: зафиксировано, является отчёт информационным шагом или gate для конкретной locale; выбранное поведение реализовано и описано в `BuildAndTooling`.
  - Evidence: В `Docs/Architecture/BuildAndTooling.md` и `.github/workflows/linux-ci.yml` шаг `gv2-content coverage GameData/core` зафиксирован как информационный (informational step, smoke command), возвращающий exit code 0 и не блокирующий `validate`, сборку и тесты (с fallback на `source_message`).

- [x] **LOC-10 — Синхронизировать документацию**
  - Зависимости: LOC-01–LOC-09.
  - Done: контракт текста описывает разделение identity и содержимого; `BuildAndTooling` описывает размещение каталогов, build-шаг и новую команду; `ImplementationStatus` обновлён.
  - Evidence: Обновлены `Docs/Architecture/BuildAndTooling.md` (размещение каталогов `<package-root>/localization/*.po`, команда `gv2-content coverage`, сборка String Table CSV, роль в CI), `Docs/Architecture/DefinitionEnvelopeAndSchemaRules.md` (семантика `source_message`), `Docs/Architecture/GameDataRepositoryContract.md` (ссылки kind `text`), `Docs/Status/ImplementationStatus.md` (статус PO-каталогов, CLI coverage, 25 entry points и резолвинга в Presentation); `validate_docs.py` проходит с 0 ошибок.

## Проверка milestone

- [x] Отчёт различает три состояния ключа.
- [x] Отсутствие перевода не влияет на валидность контента.
- [x] Роль отчёта в CI зафиксирована явно.
