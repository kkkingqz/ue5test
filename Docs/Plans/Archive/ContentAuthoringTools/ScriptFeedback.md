---
title: Script Feedback Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ../../../Architecture/LuaRuntimeContract.md
  - ../../../Architecture/HeadlessSimulationContract.md
---

# M2 — Script Feedback

## Результат этапа

Автор узнаёт за секунды, что его Lua-модуль загружается и объявлен в манифесте корректно, не запуская геймплей и не открывая UE.

## Задачи

- [x] **CAT-04 — Реализовать проверку module graph**
  - `gv2-headless --check-scripts` загружает дерево `Scripts/`, проверяет манифест, граф зависимостей и покрытие исходников, после чего завершает работу.
  - Загрузчик уже отклоняет отсутствующие, незаявленные и дублирующиеся исходники, дубликаты `module_id`, циклы и недостижимые модули — задача не добавляет правил, а даёт им отдельную точку входа.
  - Done: геймплей не исполняется и команды не диспетчеризуются; repository строится, поскольку модули могут обращаться к нему при инициализации; успешный прогон завершается быстрее секунды на текущем дереве.
  - Evidence: `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h` (`FRuntimeSession::CheckScripts`), `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Headless/Source/main.cpp` (`RunCheckScripts`, `--check-scripts`), `Headless/CMakeLists.txt` (`gv2_headless_check_scripts`), CTest (37/37 passed).

- [x] **CAT-05 — Сделать ошибку пригодной для автора**
  - Done: сообщение содержит `module_id`, package-relative путь и позицию, если ошибка синтаксическая; абсолютные пути не выводятся; коды ошибок стабильны и отличают отсутствующий исходник, незаявленную зависимость, цикл и синтаксическую ошибку; negative case на каждый код.
  - Evidence: `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` (стабильные коды `LuaModuleSourceMissing`, `LuaModuleSourceUnlisted`, `LuaModuleDependencyMissing`, `LuaModuleDependencyCycle`, `LuaModuleSyntaxError`, `LuaModuleLoadError`), `Tools/Content/test_script_feedback.py` (тесты positive/negative для каждого кода ошибки), `Headless/CMakeLists.txt` (`gv2_headless_script_feedback_python`), CTest (38/38 passed).

- [x] **CAT-06 — Включить проверку в CI и контракт**
  - Зависимости: CAT-04, CAT-05.
  - Done: добавлен CTest на успешный и на отказной путь; `BuildAndTooling` описывает флаг и его exit code; `HeadlessSimulationContract` отмечает, что режим не относится ни к parity gate, ни к replay и не расширяет роли host-а.
  - Evidence: `Docs/Architecture/BuildAndTooling.md` (документирован флаг `--check-scripts`, exit code 1, local commands), `Docs/Architecture/HeadlessSimulationContract.md` (зафиксирована роль `--check-scripts` как изолированного авторского tooling-режима), `.github/workflows/linux-ci.yml`, `Headless/CMakeLists.txt` (`gv2_headless_check_scripts`, `gv2_headless_script_feedback_python`), CTest (38/38 passed).

## Проверка milestone

- [x] Проверка скриптов не запускает геймплей.
- [x] Каждая категория ошибки имеет стабильный код и negative case.
- [x] `gv2-content` по-прежнему не линкует Lua.
