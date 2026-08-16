---
title: Localization Pipeline Implementation Plan
status: archived
version: 2.0
updated: 2026-08-15
depends_on:
  - ../../../Architecture/GameDataRepositoryContract.md
  - ../../../Architecture/LuaRuntimeContract.md
  - ../../../UI/README.md
decisions:
  - ../../../ADR/0013-unified-text-pipeline.md
  - ../../../ADR/0022-external-translation-catalog.md
---

# План реализации Localization Pipeline

> **Архив.** План выполнен полностью (M1–M4) и больше не является источником задач. Нормативное поведение перенесено в [Build and Tooling](../../../Architecture/BuildAndTooling.md), [Definition Envelope and Schema Rules](../../../Architecture/DefinitionEnvelopeAndSchemaRules.md) и [ADR-0022](../../../ADR/0022-external-translation-catalog.md). Документ сохраняется как implementation record.

## Цель

Развести идентичность текста и его содержимое: репозиторий владеет `text_id`, внешний PO-каталог владеет переводами, резолвинг принадлежит host-у. План материализует [ADR-0022](../../../ADR/0022-external-translation-catalog.md).

## Состояние на входе

Текст является definition kind `text` с обязательным `data.message`. Правка перевода меняет `content_hash`. Локализация не реализована: `TextSpec` доходит до presentation неразрешённым и отображается как есть.

## Границы

Входят: изменение схемы `text`, формат и размещение PO-каталогов, загрузка и резолвинг на стороне UE, политика fallback, инструмент отчёта о полноте перевода.

Не входят: множественные формы и род, переключение locale в рантайме, перевод строк UI-шаблонов, авторская документация для переводчиков, резолвинг в headless.

## Milestones

- [x] M1 — [Text Identity](TextIdentity.md): схема `text` хранит идентичность и исходную строку.
- [x] M2 — [Catalog Format](CatalogFormat.md): PO-каталоги внутри package root.
- [x] M3 — [Host Resolution](HostResolution.md): UE резолвит `TextSpec`, headless — нет.
- [x] M4 — [Coverage Tooling](CoverageTooling.md): отчёт о полноте, а не gate.

## Критический путь

```text
Text Identity
→ Catalog Format
→ Host Resolution
→ Coverage Tooling
```

## Общие правила выполнения

1. Переводы не входят в `content_hash`, не пересекают Lua boundary и не участвуют в run digest.
2. Отсутствующий перевод не является ошибкой: применяется fallback на исходную строку.
3. Проверка полноты перевода не встраивается в `gv2-content validate`: отсутствие перевода не делает контент невалидным.
4. Вторая реализация формата каталога запрещена; PO разбирается одним способом.
5. Новое observable behavior синхронно отражается в contract.

## Координация с другими планами

M1 однократно меняет pinned content hash фикстур и `GameData/core`. Эту задачу не стоит объединять в одном change set с задачами других планов, меняющими corpus, иначе причина смены хэша станет неотличимой.

## Итоговый Definition of Done

- [x] Опечатка в `text_id` по-прежнему является фатальной диагностикой сборки.
- [x] Правка перевода не меняет `content_hash`.
- [x] UE отображает перевод для выбранной locale, headless — исходную строку.
- [x] Отсутствующий перевод не ломает ни сборку, ни запуск.
- [x] Полнота перевода видна отчётом.
