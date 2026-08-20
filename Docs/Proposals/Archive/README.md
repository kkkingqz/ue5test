---
title: Archived Implementation Proposals
status: archived
version: 1.2
updated: 2026-08-20
depends_on:
  - ../README.md
---

# Архив реализованных предложений

Здесь хранятся предложения со статусом `proposal_state: implemented`. Архивное предложение **не является нормативным** и не источник задач: актуальное поведение описывают subsystem contracts в `Docs/Architecture` и `Docs/UI`, а решения — `accepted` ADR.

Документ сохраняется как rationale и implementation record: он отвечает на вопрос «почему сделано именно так» и какие альтернативы были отвергнуты по дороге. Разбор состояния на входе в нём описывает состояние на момент написания и с тех пор устарел по определению.

| Предложение | Реализовано планом | Результат |
|---|---|---|
| [PortableContentCoreProposal](PortableContentCoreProposal.md) | [PortableContentCore](../../Plans/Archive/PortableContentCore.md) | Общий portable pipeline `Packages → Definitions → Repository Snapshot → Runtime` |
| [CoreGameplayBoundaryProposal](CoreGameplayBoundaryProposal.md) | [CoreBoundaryMigration](../../Plans/Archive/CoreBoundaryMigration.md) | Правило ownership между framework core и gameplay packages |
| [DesignerLuaAuthoringProposal](DesignerLuaAuthoringProposal.md) | [DesignerAuthoringLayer](../../Plans/Archive/DesignerAuthoringLayer.md) | Designer-facing Lua: отложенная регистрация, три вида property, `write_revision` и правило `fail()` |
| [SimplifiedAuthoringSurfaceProposal](SimplifiedAuthoringSurfaceProposal.md) | [SimplifiedAuthoringSurface](../../Plans/Archive/SimplifiedAuthoringSurface.md) | Окружение authoring-скрипта без `M.`, автообнаружение модулей, источник презентации |
| [TextSystemLayerProposal](TextSystemLayerProposal.md) | [TextSystemLayer](../../Plans/Archive/TextSystemLayer.md) | Слой `textsystem` между движком и игрой; набор пакетов из данных |
| [EntityAuthoringExtensionProposal](EntityAuthoringExtensionProposal.md) | [EntityAuthoringExtensions](../../Plans/Archive/EntityAuthoringExtensions.md) | Декларативное расширение доменных сущностей через авторские прототипы `_ENV` |
| [CommandValidatorAuthoringProposal](CommandValidatorAuthoringProposal.md) | [CommandValidators](../../Plans/Archive/CommandValidators.md) | Designer-facing Validators как независимые read-only policies поверх существующего Command pipeline |
| [ImageResourceLookupOptimizationProposal](ImageResourceLookupOptimizationProposal.md) | — | Immutable $O(1)$ lookup и однократная подготовка resolved brush |
| [RHActorsLuaSimplificationProposal](RHActorsLuaSimplificationProposal.md) | [RHActorsSimplification](../../Plans/Archive/RHActorsSimplification.md) | Контракты полей `field.*`, композиция схем, обобщённое создание экземпляров |
| [GameplayServiceAuthoringProposal](GameplayServiceAuthoringProposal.md) | [GameplayServices](../../Plans/Archive/GameplayServices.md) | Авторский синтаксис `services.<name> = { … }` для stateless-процессов и торговец в `rh` |
| [ContentEditorPluginProposal](ContentEditorPluginProposal.md) | [ContentEditor](../../Plans/Archive/ContentEditor.md) | Unreal Editor frontend поверх общей atomic authoring library и канонических JSON5 definitions |
