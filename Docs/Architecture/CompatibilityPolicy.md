---
title: Compatibility Policy
status: normative
version: 1.0
updated: 2026-08-20
depends_on:
  - StableIDSpecification.md
  - CanonicalStateAndSave.md
  - DefinitionEnvelopeAndSchemaRules.md
  - Modding.md
decisions:
  - ../ADR/0036-pre-1-0-compatibility-policy.md
---

# Compatibility Policy

> **Владеет:** публичными гарантиями совместимости и границей, с которой они действуют.
> **Не владеет:** форматом сейва, правилами schema evolution, package manifest и жизненным циклом Stable ID.
> **Инварианты:** [INV-017](Invariants.md), [INV-009](Invariants.md)
> **Реализация:** project version — `version` обязательного `GameData/core/package.json5`; локальные версии и migrations принадлежат соответствующим contracts.
> **Проверки:** release review, package/content validation и migration fixtures затронутой оси.

## Граница гарантии

Canonical project version — SemVer-поле `version` пакета `core`. Версии отдельных пакетов и целочисленные диапазоны `compatibility.game/api/schema` из [Modding](Modding.md) решают локальные задачи и не заменяют project version.

- До `1.0.0` проект не гарантирует обратную совместимость между релизами.
- Начиная с `1.0.0` совместимость следует таблице ниже.

Отсутствие гарантии до `1.0.0` не разрешает silent breaking change. Изменение обязано быть классифицировано, версионировано и сопровождаться нужными migrations, diagnostics, документацией и тестами в одном change set.

## Оси совместимости

| Ось | До `1.0.0` | Начиная с `1.0.0` |
|---|---|---|
| Save | Поддерживается только явно заявленное окно версий. Остальное отклоняется typed error | Релиз обязан загружать сейвы более ранних релизов того же major через явные migrations; отказ требует нового major |
| Content schemas | Breaking change допустим с новой `schema_version` и явным переводом контента | Новые версии могут сосуществовать; удаление либо несовместимое изменение поддерживаемой публичной версии требует нового project major |
| Package и Lua API | Breaking change допустим с синхронным обновлением затронутых пакетов и compatibility ranges | Публичный API не ломается внутри project major |
| Stable ID | Опубликованный ID не меняет смысл и не переиспользуется | То же правило без исключений |

Текущие обязательства `save_version`, `codec_version`, section versions, `schema_version` и package compatibility ranges действуют независимо от project version, пока их owner contract явно не изменён.

## Процедура изменения

Breaking change обязан:

1. Назвать затронутые оси и поддерживаемое окно.
2. Обновить project/local versions и compatibility ranges, где применимо.
3. Добавить migration либо typed отказ без частичного применения и silent fallback.
4. Обновить owner contracts и fixtures.

Compatibility alias, скрытая миграция и переиспользование Stable ID запрещены. Исключение требует отдельного ADR; правило публикации ID из [Stable ID Specification](StableIDSpecification.md) не ослабляется даже новым major.

## Acceptance criteria

- Единственный project version читается из `GameData/core/package.json5` и имеет SemVer-форму.
- Любой breaking change явно классифицирован по таблице.
- Неподдерживаемые save/content/package версии дают typed diagnostic до публикации state или repository.
- Поддерживаемые migrations имеют positive и negative fixtures.
