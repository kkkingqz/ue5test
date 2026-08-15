---
title: Command Validators Tasks
status: draft
version: 1.3
updated: 2026-08-15
depends_on:
  - README.md
  - ../../Architecture/CommandsAndEvents.md
decisions:
  - ../../ADR/0003-command-and-event-model.md
---

# M1 — Command Validators

## Результат этапа

Между приёмом команды и вызовом handler появляется ordered read-only проверка. Отказ валидатора является нормальным результатом, а не сбоем, и гарантированно не оставляет следов в state.

## Задачи

- [x] **GEW-01 — Реестр валидаторов и порядок**
  - Регистрация на фазе `register`, freeze вместе с остальными registries.
  - Порядок исполнения: priority, затем package load order, затем registration order.
  - Done: порядок воспроизводим и покрыт conformance-набором с несколькими валидаторами; поздняя регистрация после freeze отклоняется; валидатор адресуется Stable ID kind `validator`.
  - Evidence: `Scripts/runtime/validator_registry.lua` (`M.create_registry()`: `register`/`get`/`exists`/`freeze`/`is_frozen`/`ordered`, `M.run_conformance()`); подключён в `Scripts/bootstrap/manifest.lua`/`main.lua`; экспортируется как `game.commands.validators` (реализует ранее пустое поле фасада `commands`, [LuaRuntimeContract](../../Architecture/LuaRuntimeContract.md#gamecommandsvalidators-gew-01)). C++ freeze — `FGV2SessionCoordinator`/`GV2RuntimeSession.cpp` `FreezeGameRegistry({"commands", "validators"})` (обобщённый helper, тот же вызов для `game.services`). Portable conformance entry point `GV2RuntimeCore::Testing::RunValidatorRegistryConformance()` (`GV2ValidatorRegistryConformance.h/.cpp`) стартует изолированную Lua-сессию с реальным модулем, проверяет порядок (priority ascending, tie-break по registration order), rejection позднего/дублирующего/невалидного (не kind `validator`) id, а также live-заморозку `game.commands.validators` после register-фазы; вызывается из `gv2-headless --self-test` и UE `GV2.Runtime.Lua.ValidatorRegistryConformanceCrossHost` — оба зелёные (CTest 21/21, UE automation 46/46). Package load order отдельно не отслеживается: единственный global register-проход над уже разрешённым `LoadOrder` делает его композицией с registration order (пояснено в комментарии модуля и в `LuaRuntimeContract.md`) — mod-пакеты вне scope плана.

- [x] **GEW-02 — Ограничить права валидатора**
  - Валидатор читает state, repository и payload; не мутирует, не публикует события, не запускает операции.
  - Done: попытка изменить state из валидатора отклоняется mutation window, поскольку окно не открывается до handler; ошибка внутри валидатора является runtime fault, а не обычным отказом, и отличается от него по typed-результату.
  - Evidence: `Scripts/runtime/command_dispatcher.lua` (`run_validators`, вызывается до `mutation_window.execute_in_window`; каждый validator получает `{ state, repository, payload, command_id }`). Permission enforcement переиспользует существующий `mutation_window` — `state.x = ...` из validator-а бросает `MutationWindowClosed`, отдельного кода не потребовалось; `game.events`/`game.bridge.start_operation` физически недостижимы (не реализованы), поэтому запрет "не публикует события/не запускает операции" верен без явной проверки. Typed refusal (`return false, {code=...}`) не бросает ошибку → dispatch успешен, `command_result={ok=false,error=...}`, handler пропущен; ошибка/попытка мутации бросает Lua error → dispatch fails (`FRuntimeSession::DispatchCommand` возвращает `false` + `Fault.Code="LuaDispatchError"`) — типы результатов различимы на host-уровне. Portable conformance entry point `GV2RuntimeCore::Testing::RunCommandValidatorInvocationConformance()` (`GV2CommandValidatorInvocationConformance.h/.cpp`) стартует изолированную сессию с реальным `command_dispatcher.lua`/`validator_registry.lua`/`mutation_window.lua` и через настоящий `FRuntimeSession::DispatchCommand` boundary (не внутренний Lua-вызов) проверяет все 4 сценария (read access + allow, allow без ограничений, typed refusal без мутации, mutation attempt → fault без мутации) по `GetCanonicalStateHash()` до/после; вызывается из `gv2-headless --self-test` и UE `GV2.Runtime.Lua.CommandValidatorInvocationConformanceCrossHost` — оба зелёные (CTest 21/21 включая golden digest, UE automation 47/47).

- [x] **GEW-03 — Зафиксировать семантику отказа**
  - Отказ возвращает `{ ok = false, error = { code = "core:error....", params = {} } }` со стабильным Stable ID kind `error`.
  - Done: после отказа state не изменён и хэш state не изменился; handler не вызывался; первый отказавший валидатор прекращает цепочку; negative case на каждое из трёх утверждений.
  - Evidence: `Scripts/runtime/command_dispatcher.lua` (`normalize_refusal`: `refusal.code` проверяется `stable_id.is_kind(code, "error")`, `params` по умолчанию `{}`, отсутствующий `refusal` целиком даёт `core:error.command.validation_refused`; невалидный `code`/`params` бросает `InvalidValidatorRefusal`, что становится `LuaDispatchError` тем же путём, что и любая другая ошибка validator-а). `run_validators` возвращается сразу на первом `false` — остальные validators не вызываются. Portable conformance entry point `GV2RuntimeCore::Testing::RunCommandRefusalSemanticsConformance()` (`GV2CommandRefusalSemanticsConformance.h/.cpp`) через реальный `FRuntimeSession::DispatchCommand` boundary проверяет: (1) well-formed refusal — dispatch succeeds, `GetCanonicalStateHash()` до/после не меняется, handler не вызван; (2) chain из двух validators — второй (что бы попытался мутировать state, если бы был вызван) не вызывается, что доказывается тем, что dispatch остаётся успешным, а не fault; (3) невалидный `code` (не Stable ID kind `error`) — dispatch fails с `LuaDispatchError`/`InvalidValidatorRefusal`, state не изменён; (4) отсутствующий `params` по умолчанию `{}` — проверено Lua-internal assertion в register/start hook (нет C++ readback для содержимого `command_result`), провал assertion превращается в session-start failure. Вызывается из `gv2-headless --self-test` и UE `GV2.Runtime.Lua.CommandRefusalSemanticsConformanceCrossHost` — оба зелёные (CTest 21/21 включая golden digest, UE automation 48/48).

## Проверка milestone

- [x] Порядок валидаторов воспроизводим и одинаков в обоих host-ах (`GV2.Runtime.Lua.ValidatorRegistryConformanceCrossHost`).
- [x] Отказ не меняет state и не вызывает handler (`GV2.Runtime.Lua.CommandValidatorInvocationConformanceCrossHost`, `GV2.Runtime.Lua.CommandRefusalSemanticsConformanceCrossHost`).
- [x] Ошибка валидатора отличима от отказа (`LuaDispatchError`+fault vs `ok=false` typed refusal; те же тесты).
