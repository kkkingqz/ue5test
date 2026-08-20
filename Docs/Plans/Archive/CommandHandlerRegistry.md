---
title: CommandHandlerRegistry Archive Summary
status: archived
version: 1.0
updated: 2026-08-16
---

# CommandHandlerRegistry: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Заменить упорядоченную цепочку обработчиков реестром с ключом `command_id`. Это последнее место, где добавление геймплейной команды требует правки файлов ядра, и последнее, где оно требует C++

**Результат:** обработчик команды регистрируется по `command_id`, и пакет добавляет команду, не трогая ядро

## Этапы и задачи

### M1 — Registry

реестр обработчиков существует, замораживается вместе с остальными и покрыт спеками

- `CHR-01` — Создать модуль реестра
- `CHR-02` — Подключить реестр и его заморозку
- `CHR-03` — Покрыть реестр спеками

### M2 — Dispatch by Key

команда находится за один lookup, неизвестная отклоняется, точка входа не знает игру

- `CHR-04` — Перевести диспетчер на lookup
- `CHR-05` — Отклонять неизвестную команду
- `CHR-06` — Конвертировать обработчики ядра и развязать `ingress`
- `CHR-07` — Подтвердить отсутствие дрейфа поведения

### M3 — Package Commands

пакет добавляет команду, не трогая ядро, и это зафиксировано контрактом

- `CHR-08` — Команда из пакета
- `CHR-09` — Перекрытие команды пакетом
- `CHR-10` — Синхронизировать документацию

## Актуальные нормативные источники

- [CommandsAndEvents](../../Architecture/CommandsAndEvents.md)
- [RuntimeFacadeAndRegistries](../../Architecture/RuntimeFacadeAndRegistries.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/CommandHandlerRegistry) содержит исходные task-файлы, acceptance criteria и evidence.
