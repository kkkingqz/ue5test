---
title: "ADR-0016: PNG Suffix Image Metadata"
status: accepted
date: 2026-08-12
---

# ADR-0016: PNG Suffix Image Metadata

## Context

Отдельный JSON sidecar для каждого bitmap создаёт пару файлов, которая может рассинхронизироваться. Для трёх утверждённых render mode достаточно однозначно определить режим по имени PNG, а nine-slice границы хранить в стандартной однопиксельной marker-рамке самого изображения.

## Decision

- `name.png` задаёт `fixed_aspect`; ratio равен decoded width/height.
- `name.tile.png` задаёт `tile`; logical tile size равен decoded width/height.
- `name.9.png` задаёт `nine_slice`. Внешняя однопиксельная рамка не входит в runtime texture. Один непрерывный чёрный marker на верхней границе задаёт горизонтально растягиваемый interval, один marker на левой — вертикальный.
- Суффиксы `.tile` и `.9` являются authoring metadata и удаляются при построении `resource_id`.
- JSON sidecar для image render metadata не поддерживается.
- Duplicate source files, которые после удаления suffix дают одинаковый `resource_id`, делают сборку candidate catalog ошибочной.

## Consequences

- Новый image resource остаётся одним переносимым файлом.
- Tile period следует physical bitmap dimensions; изменение dimensions опубликованного tile меняет его presentation contract.
- Nine-slice PNG обязан иметь валидные marker runs и положительную центральную область после удаления рамки.
- Добавление новых suffix или изменение их смысла требует ADR и синхронного изменения validators.

## Rejected alternatives

- JSON sidecar: допускает рассинхронизацию и избыточен для текущих трёх режимов.
- PNG textual chunks: неодинаково поддерживаются графическими редакторами и хуже видны в обычном file workflow.
- Фиксированные border proportions по соглашению: недостаточны для панелей с различной геометрией углов.
