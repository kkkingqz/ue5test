---
title: "ADR-0036: Pre-1.0 Compatibility Policy"
status: accepted
date: 2026-08-20
---

# ADR-0036: Pre-1.0 Compatibility Policy

> **Решение:** project version хранится в descriptor пакета `core`; до `1.0.0` публичной гарантии обратной совместимости нет, но breaking changes остаются явными и проверяемыми.
> **Нормативный текст:** [Compatibility Policy](../Architecture/CompatibilityPolicy.md).

## Context

Save, schemas и package API уже имеют локальные версии, но проект не определял общую границу совместимости. Формула «до 1.0 всё можно ломать» допускала бы тихую смену смысла данных и не объясняла, какая версия является версией проекта.

## Decision

1. Canonical project version — SemVer-поле `version` в обязательном `GameData/core/package.json5`.
2. До `1.0.0` обратная совместимость между релизами не обещается.
3. Breaking change до `1.0.0` всё равно требует явной классификации, версий, migration или typed отказа и тестов.
4. С `1.0.0` saves и public API совместимы внутри major; breaking change требует нового major.
5. Опубликованный Stable ID не переиспользуется ни при какой project version.

## Consequences

Release и review получают одну проверяемую границу. Локальные версии сохраняют своё назначение; project version их не заменяет. До `1.0.0` разрешено быстро менять архитектуру без неявной порчи данных.

## Rejected alternatives

- Отдельный файл версии: дублирует обязательный descriptor `core`.
- Полное отсутствие политики до 1.0: превращает silent breakage в допустимое поведение.
- Вечная совместимость каждого pre-1.0 релиза: несоразмерна стадии проекта.
