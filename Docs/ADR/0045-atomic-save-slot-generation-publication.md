---
title: "ADR-0045: Atomic Save Slot Generation Publication"
status: accepted
date: 2026-09-12
---

# ADR-0045: Atomic Save Slot Generation Publication

> **Решение:** slot хранит immutable physical generations, а единственной commit point является атомарная подмена маленького storage-owned head, указывающего на `Current` и `Previous`. Lua save bytes остаются непрозрачными для host-а.

## Context

ADR-0021 требует атомарной записи opaque bytes и сохранения предыдущей копии. Простая последовательность «переименовать current в backup, затем temp в current» имеет две commit-like границы: crash между ними оставляет неоднозначное состояние, а два writer-а могут использовать одинаковый temp path. Копирование current после публикации нового файла способно сохранить в backup уже новые bytes.

Нужна одна наблюдаемая точка публикации без разбора save container и без общего transaction framework.

## Decision

### D1 — Immutable generations и head

Каждая успешная подготовка создаёт новый уникально именованный generation file внутри области конкретного slot. После закрытия generation неизменяем. Versioned bounded head содержит только storage metadata: имя `Current` generation и optional имя `Previous` generation. Gameplay metadata и container integrity в head запрещены.

Строгая grammar head отклоняет unknown version, неизвестные поля, небезопасные имена и выход из slot scope как `Unreadable`. Host не пытается выбрать «наиболее валидный» Lua container.

### D2 — Единственная commit point

Write выполняет:

1. записать новые opaque bytes в уникальный temp generation;
2. flush/close и переименовать его в immutable generation;
3. записать temp head `(new_generation, old_current)`;
4. атомарно заменить опубликованный head одним rename на том же volume.

Шаг 4 — единственная commit point. Ошибка до него сохраняет прежние `Current`/`Previous`; ошибка cleanup после него не превращает committed write в failure. При первом write `Previous` отсутствует. Следующий startup удаляет только generations/temp files, на которые не ссылается успешно прочитанный head.

### D3 — Single writer и сериализованные операции

Успешно открытый storage instance владеет exclusive process lock на root. Второй writer получает typed `Busy`. Внутри owner чтения, publication и cleanup сериализованы, поэтому reader не наблюдает удаление выбранной generation во время чтения. Lock lifetime совпадает с application-owned storage lifetime, а не с session.

### D4 — Явный выбор revision

API читает `Current | Previous`. Отсутствующий `Previous` возвращает `NotFound`; автоматический fallback запрещён. Load request захватывает выбранные bytes ровно один раз и передаёт тот же immutable buffer в Lua preflight A и bootstrap B.

### D5 — Baseline и legacy migration

Первая гарантия относится к Linux filesystem, где temp и target находятся на одном volume и rename атомарен. Проверяется process crash, но не power loss; durability с `fsync` файла и каталога требует отдельного решения.

Существующий legacy single-current slot без head читается как `Current`. Первый overwrite сначала переносит его bytes в immutable generation и только затем публикует head, где legacy bytes становятся `Previous`. Ошибка до head commit оставляет legacy slot читаемым. Это migration размещения opaque bytes, не migration Lua container.

## Consequences

- После successful overwrite `Current` всегда содержит новые bytes, `Previous` — непосредственно предшествующие committed bytes.
- C++ получает небольшую собственную storage schema, но не получает знания о canonical state или save container.
- Уникальные temp/generation names и exclusive owner устраняют collision между конкурентными writers.
- Recovery выбирает revision явно; пригодность bytes определяет только Lua preflight.
- Fault/crash matrix может перечислять фактически выполненные filesystem stages через один internal adapter.

## Rejected alternatives

- **Rename current → backup → temp → current.** Имеет две наблюдаемые границы и неоднозначный crash state.
- **Копировать backup после публикации current.** Может скопировать уже новое поколение и потерять предыдущее.
- **Разбирать container и выбирать valid revision в C++.** Нарушает opaque boundary ADR-0021.
- **Один постоянный temp filename без lock.** Допускает race и повреждение между процессами.
- **Обещать power-loss durability без fsync protocol.** Это более сильная гарантия, чем поддерживает принятый baseline и планируемая проверка.
