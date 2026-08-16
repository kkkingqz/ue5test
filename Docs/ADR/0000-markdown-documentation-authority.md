---
title: "ADR-0000: Markdown Documentation Authority"
status: accepted
date: 2026-08-10
---

# ADR-0000: Markdown Documentation Authority

> **Решение:** Markdown в `Docs` — единственный нормативный набор.
> **Нормативный текст:** [Docs/README](../README.md).

## Context

Исходные DOCX появились в разное время и содержали несовместимые grammar, terminology и runtime contracts. Одновременная нормативность нескольких копий увеличивает drift.

## Decision

`Docs/**/*.md` — единственный нормативный набор для реализации и AI. Legacy DOCX удалены; экспортированные, архивные и внешние копии не хранятся рядом с canonical Markdown и не используются для implementation decisions. Accepted ADR и subsystem contracts задают precedence согласно `Docs/README.md`.

## Consequences

- Один searchable/linkable source of truth.
- Примеры и cross-links проверяются автоматически.
- Отсутствует конкурирующая локальная копия, которую можно ошибочно принять за актуальный contract.

## Rejected alternatives

- Поддерживать DOCX и Markdown как равноправные копии: неизбежный drift.
- Выбирать документ только по дате: не фиксирует локальные supersession decisions.
