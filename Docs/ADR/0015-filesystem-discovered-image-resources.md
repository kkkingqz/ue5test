---
title: "ADR-0015: Filesystem-Discovered Image Resources"
status: superseded
date: 2026-08-12
superseded_by: 0016-png-suffix-image-metadata.md
---

# ADR-0015: Filesystem-Discovered Image Resources

Решение заменено [ADR-0016](0016-png-suffix-image-metadata.md): filesystem discovery и directory-derived `resource_id` сохранены, JSON sidecar заменён filename suffix и marker border самого PNG.

## Context

Manual registration каждой PNG в UE Data Asset создаёт лишнюю editor operation и мешает добавлению большого количества core/mod graphics. Stable ID уже способен однозначно следовать package-relative directory layout.

## Decision

- При startup UE рекурсивно сканирует project-relative `Resources` directory и атомарно строит immutable Image Resource Catalog.
- Canonical path `Resources/<namespace>/resource/<path>.png` создаёт `<namespace>:resource.<path>`, заменяя directory separators на dots и удаляя `.png`.
- Поддерживаются только lowercase `.png` и canonical lowercase ASCII directory/file segments.
- PNG без sidecar является `fixed_aspect`; ratio вычисляется из decoded source width/height. Optional `<name>.resource.json` может явно закрепить ratio или выбрать `nine_slice`/`tile` и обязательные metadata.
- Lua и portable DTO по-прежнему передают только `resource_id`; filesystem path и render metadata не пересекают boundary.
- Scan/decode/metadata validation выполняются в candidate catalog. Любая ошибка отменяет публикацию всего candidate.
- Resource files stage-ятся как `NonUFS`, чтобы тот же scanner работал в packaged build. Live rescan active session отсутствует.

## Consequences

- Добавление PNG не требует создания `.uasset` или изменения C++/Blueprint.
- Directory rename меняет Stable ID и считается content migration, а не cosmetic refactor.
- `fixed_aspect` ratio следует source bitmap, если sidecar не закрепляет значение; принимающий block всё равно обязан проверить matching ratio.
- Sidecar использует strict JSON UTF-8. Он хранит только presentation metadata и не является gameplay Definition.
- Три режима `fixed_aspect`, `nine_slice`, `tile` из ADR-0014 сохраняются; четвёртый режим по-прежнему требует нового ADR.

## Rejected alternatives

- Editor auto-import в Content Browser: создаёт derived `.uasset`, source-control churn и отдельный mapping step.
- Manual Data Asset entry на каждый файл: дублирует directory identity.
- Filename encoding borders/tile size: плохо читается и ограничивает evolution metadata.
- Runtime rescan во время active session: нарушает pinned presentation resource snapshot.
