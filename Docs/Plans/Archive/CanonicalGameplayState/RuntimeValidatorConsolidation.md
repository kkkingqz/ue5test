---
title: Runtime Validator Consolidation Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - StateObservability.md
  - ../../../Architecture/LuaRuntimeContract.md
  - ../../../Architecture/BuildAndTooling.md
decisions:
  - ../../../ADR/0020-cpp-scope-criterion.md
---

# M4 — Runtime Validator Consolidation

## Результат этапа

Правила валидации canonical state существуют в одном экземпляре, в `Scripts/`, и не дублируются внутри C++. Список канонических секций объявлен один раз. Возврат дублирующей реализации обнаруживается автоматически.

## Исходное состояние

M1 оставил два независимых набора правил:

- `Scripts/runtime/state_validator.lua` (211 строк) загружается модулем и экспортируется из `bootstrap/main.lua`, но `M.validate_state_tree` не вызывается ниоткуда — это мёртвый код;
- живая валидация выполняется Lua-чанком, встроенным строковым литералом в `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`; файл вырос с 1273 до 1939 строк.

Список канонических секций объявлен четырежды: в C++ массиве, во встроенном Lua, в `state_validator.lua` и в conformance-наборе.

Оба набора проверяют пересекающиеся правила разными реализациями. Это тот же класс дефекта, ради которого строился parity gate, но гейт его не видит: он запрещает только host-локальные `Run*SelfTest` в `Headless/Source/main.cpp`.

Отдельно это расходится с ADR-0020: gameplay-правила, выраженные на Lua, размещены в C++-файле без выполнения любого из двух условий принадлежности C++.

## Задачи

- [x] **CGS-15 — Свести валидацию state к одной реализации**
  - Живые правила переносятся в `Scripts/runtime/state_validator.lua`; встроенный в C++ Lua-чанк удаляется. Host вызывает модуль, а не собственную копию правил.
  - Перенос не меняет содержание проверок: сначала перенос без изменения поведения, расширение покрытия — отдельно.
  - Done: `M.validate_state_tree` вызывается на фактическом пути bootstrap; ни одна проверка state не остаётся в C++; поведение и коды ошибок не изменились, что подтверждается существующими conformance-наборами.
  - Evidence: `GV2RuntimeSession.cpp`, `GV2LuaLifecycleConformance.cpp` (`GV2.Runtime.Lua.LifecycleConformanceCrossHost`), CTest (21/21 passed).

- [x] **CGS-16 — Объявить список канонических секций один раз**
  - Зависимости: CGS-15.
  - Единственный источник — Lua; C++ и conformance-наборы получают список оттуда либо не знают его вовсе.
  - Done: добавление или переименование секции требует правки одного места (`CANONICAL_SECTIONS` в `Scripts/runtime/state_validator.lua`); рассинхронизация невозможна по построению; C++ динамически запрашивает секции и генерирует начальное дерево через `state_validator.create_empty_canonical_state` и `state_validator.is_canonical_section`.
  - Evidence: `Scripts/runtime/state_validator.lua`, `GV2RuntimeSession.cpp`, `GV2LuaLifecycleConformance.cpp`, CTest (21/21), UE automation (45/45).

- [x] **CGS-17 — Расширить parity gate на встроенный Lua**
  - Зависимости: CGS-15.
  - `Tools/Content/validate_host_conformance_parity.py` сейчас ловит только host-локальные self-тесты в headless. Gameplay-логика, встроенная строковым литералом в C++, им не обнаруживается.
  - Done: Lua-чанк с gameplay- или validation-правилами внутри production C++ ломает CI; Lua-исходники внутри conformance-файлов остаются разрешены как тестовые фикстуры и отличаются от production по явному признаку.
  - Evidence: `Tools/Content/validate_host_conformance_parity.py`, CTest (Test #2: `host_conformance_parity_contract`).

- [x] **CGS-18 — Синхронизировать документацию этапа**
  - Зависимости: CGS-15–CGS-17.
  - Done: `BuildAndTooling` описывает новое правило гейта; `LuaRuntimeContract` называет `state_validator` единственным владельцем правил валидации state; `ImplementationStatus` / `Docs/Plans/CanonicalGameplayState/README.md` обновлён.
  - Evidence: `Docs/Architecture/BuildAndTooling.md`, `Docs/Architecture/LuaRuntimeContract.md`, `Docs/Plans/CanonicalGameplayState/README.md`.

## Проверка milestone

- [x] Правила валидации state существуют в одном файле.
- [x] Мёртвого дубликата validator-а не осталось.
- [x] Список секций объявлен один раз.
- [x] Возврат gameplay-правил в C++ обнаруживается CI.
