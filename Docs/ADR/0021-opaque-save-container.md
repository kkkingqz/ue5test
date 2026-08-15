---
title: "ADR-0021: Opaque Save Container"
status: accepted
date: 2026-08-14
---

# ADR-0021: Opaque Save Container

## Context

`CanonicalStateAndSave` фиксировал export boundary так: Lua формирует pure data
tree, а C++ валидирует допустимые DTO-типы, depth/size policy и envelope,
сериализует payload и метаданные, считает checksum, пишет временный файл и
атомарно подменяет slot. Load требовал C++-preflight header/checksum/types/mod
metadata до teardown текущей session.

Такая граница требует, чтобы всё canonical gameplay-state пересекало
C++/Lua boundary разобранным деревом. Практически это означает:

- глубокий Lua → portable value reader в marshaller (сегодня существует только
  `ReadFlatScalarObject` для плоского объекта скаляров);
- C++-реализацию канонической сериализации, checksum, версионирования секций и
  оркестрации миграций;
- дублирование знания о форме state в двух языках.

Это прямо противоречит правилу ADR-0020 о минимальном представлении на boundary:
непрозрачные байты решают задачу, а разобранное дерево — нет.

Ограничение «не более одной active session и одной Lua VM» не мешает
Lua-side preflight: входящий сейв читает и проверяет **текущая** session до
запроса replacement, а не вторая параллельная VM.

## Decision

- Canonical gameplay-state не пересекает C++/Lua boundary. Ни save, ни digest,
  ни диагностика не передают state tree в C++.
- Lua владеет сериализацией state, integrity check содержимого, версиями секций
  и миграциями. Результат — непрозрачная последовательность байт.
- C++ предоставляет slot-scoped storage primitive: чтение и запись байт по
  идентификатору слота, атомарная подмена, сохранение предыдущей копии.
  Хост не разбирает содержимое и не знает его формата.
- Lua не получает произвольный filesystem access: адресация только по slot ID,
  разрешение пути и его ограничение остаются в C++.
- Preflight входящего сейва выполняет текущая active session до запроса
  replacement session.
- Physical encoding остаётся деталью реализации Lua и не является частью
  gameplay contract.

## Consequences

- Marshaller не нуждается в глубоком Lua → portable reader; boundary остаётся
  узким.
- Объём C++, необходимый для save/load, сокращается примерно с полутора тысяч
  строк до storage primitive.
- Форма state остаётся описанной в одном месте — в Lua.
- Внешний инспектор сейвов (аналог `gv2-content inspect`) становится невозможен
  без запуска Lua: инструмент обязан использовать тот же runtime. Если такой
  инструмент понадобится, он строится как Lua-host, а не как C++-парсер.
- C++ не может отклонить структурно повреждённый сейв до Lua: хост проверяет
  только доступность и целостность файла на уровне хранилища.
- Ответственность за обнаружение повреждения и за отказ применять несовместимый
  сейв полностью принадлежит Lua и обязана быть покрыта conformance-тестами.

## Rejected alternatives

- Сохранить C++-валидацию дерева: требует глубокого marshalling обеих
  направлений и дублирует знание о форме state; противоречит ADR-0020.
- Дать Lua прямой filesystem access: нарушает trust model из
  `Architecture/Overview.md`.
- Хранить сейв как repository-подобную структуру, разбираемую `GV2ContentCore`:
  смешивает immutable content pipeline с mutable runtime state и делает формат
  сейва зависимым от content schema rules.
