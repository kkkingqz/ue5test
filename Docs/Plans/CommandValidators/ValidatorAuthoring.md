---
title: Validator Authoring Tasks
status: normative
version: 1.0
updated: 2026-08-19
depends_on:
  - ArgumentDecoding.md
  - ../../Architecture/CommandsAndEvents.md
decisions:
  - ../../ADR/0027-designer-lua-authoring-layer.md
  - ../../ADR/0028-simplified-authoring-surface.md
---

# M2 — Validator Authoring

> **Материализует:** разделы «Предлагаемый API», «Идентичность Validator», «Разрешение target Command» и «`fail()` и execution scope» [предложения](../../Proposals/CommandValidatorAuthoringProposal.md).
> **Задачи:** CVA-04…08.
> **Результат:** пакет объявляет policy для чужой команды одной строкой; отказ атрибутируется объявившему пакету.

## Результат этапа

`validate(command_ref, name, fn)` доступен в авторском окружении. Валидатор получает те же аргументы, что обработчик, и отказывает через `fail()`.

## Задачи

- [ ] **CVA-04 — Общий execution scope**
  - Зависимости: CVA-03.
  - Сейчас `fail()` опирается на частный флаг `active_command_context` и вне обработчика команды даёт `AuthoringFailOutsideCommand`.
  - Done: введён общий scope `none | command | validator | event`, хранящий `kind`, `package_id`, `command_id` при применимости и начальный `write_revision` для обработчика; обёртка устанавливает scope до вызова и восстанавливает предыдущий в finally-подобном пути при успехе, типизированном отказе и исключении; спека проверяет восстановление во всех трёх случаях.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/`.

- [ ] **CVA-05 — `fail()` в scope валидатора**
  - Зависимости: CVA-04.
  - Done: `fail(key, params)` из валидатора создаёт код отказа в пространстве имён объявившего пакета и выполняет non-local exit тем же внутренним sentinel, что обработчик; обёртка перехватывает только этот sentinel и возвращает реестру `false, { code, params }`; любая другая ошибка Lua остаётся fault и приводит к `LuaDispatchError`; проверка `fail()` после мутации сохраняется только для scope `command`.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/command_validators.lua`.

- [ ] **CVA-06 — API `validate` и идентичность**
  - Зависимости: CVA-05.
  - Done: `validate(command_ref, validator_name, validator_fn)` доступен в `_ENV`; `command_ref` принимает `CommandDescriptor` и canonical Stable ID kind `command`, произвольная строка и runtime-контекст отклоняются; ID строится как `<declaring_package>:validator.<target_namespace>.<target_command_path>.<name>` и **считается непрозрачным** — связь с target хранится полем записи и доступна через интроспекцию; пара «target + name» уникальна внутри пакета, повтор даёт `AuthoringValidatorDuplicate`; ошибки `InvalidAuthoringValidatorCommand`, `InvalidAuthoringValidatorName`, `InvalidAuthoringValidatorFunction` покрыты спеками.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/command_validators.lua`.

- [ ] **CVA-07 — Разрешение target на заморозке**
  - Зависимости: CVA-06.
  - Модули обнаруживаются автоматически, поэтому проверка «target уже зарегистрирован» в момент объявления сделала бы порядок файлов частью контракта, а переименование файла — отказом bootstrap.
  - Done: объявления накапливаются на фазе `register` с нормализацией `command_ref` в canonical `command_id`; существование target проверяется однократно на общей заморозке, когда зарегистрированы обработчики всех модулей; отсутствующий target даёт `AuthoringValidatorTargetMissing` с указанием пакета и target ID; объявление после заморозки даёт `AuthoringValidatorDeclarationAfterFreeze`; спека подтверждает, что валидатор для команды из модуля, загружаемого позже, регистрируется успешно.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/command_validators.lua`.

- [ ] **CVA-08 — Адаптер к существующему реестру**
  - Зависимости: CVA-07.
  - Done: каждый авторский валидатор компилируется в реализацию текущего programmer API с локальным фильтром по `command_id`; `validator_registry.lua` и `command_dispatcher.lua` не изменены; порядок остаётся нормативным порядком реестра, порядок объявления внутри модуля сохраняется; первый типизированный отказ останавливает цепочку и обработчик не вызывается; спека покрывает несколько валидаторов, детерминированный порядок и отсутствие вызова обработчика при отказе.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/command_validators.lua`.

## Проверка milestone

- [ ] Пакет объявляет policy для чужой команды без доступа к runtime-контексту.
- [ ] Отказ несёт пространство имён объявившего пакета, а не владельца команды.
- [ ] Произвольная ошибка Lua в валидаторе остаётся fault.
- [ ] Отсутствующий target обнаруживается независимо от порядка модулей.
- [ ] `validator_registry.lua`, `command_dispatcher.lua` и C++ не изменены.
