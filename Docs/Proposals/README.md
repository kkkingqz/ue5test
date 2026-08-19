---
title: GV2 Implementation Proposals Index
status: normative
version: 3.1
updated: 2026-08-19
---

# Индекс предложений по реализации (Proposals)

В этом каталоге содержатся проработанные технические предложения по доработке, оптимизации и расширению подсистем GV2 до их прямого внедрения в код.

Proposal не изменяет нормативную архитектуру сам по себе. Поле `proposal_state: accepted_for_planning` означает, что направление принято в backlog, но до реализации продолжает действовать текущий contract и набор `accepted` ADR. `proposal_state: measurement_required` означает, что реализация ещё не разрешена: сначала нужен указанный в proposal benchmark/evidence gate. Если реализация меняет устойчивый инвариант, public contract или dependency direction, сначала создаётся ADR и синхронно обновляются затронутые contracts.

`proposal_state: implemented` означает, что предложение реализовано, а нормативное итоговое поведение перенесено в указанные subsystem contracts; proposal сохраняется как rationale и implementation record.

Трудоёмкость в документах относительная:

- **S** — локальное изменение без нового shared subsystem;
- **M** — несколько компонентов и обязательные integration tests;
- **L** — новый shared subsystem или несколько host integrations.

## Карта предложений

| Документ | Статус | Затронутые подсистемы | Описание |
|---|---|---|---|
| [ExternalProjectAdoptionProposal](ExternalProjectAdoptionProposal.md) | accepted for planning | Architecture, Dependencies | Матрица прямого использования, reference-only и отложенных внешних решений |
| [PortableContentCoreProposal](PortableContentCoreProposal.md) | implemented | Content, Runtime, Headless | Общий portable pipeline `Packages → Definitions → Repository Snapshot → Runtime` |
| [ContentDiagnosticsAndToolingProposal](ContentDiagnosticsAndToolingProposal.md) | accepted for planning | Content, CI, Tooling | Source spans, deterministic diagnostics, CLI validation, fuzzing и будущий LSP |
| [ModPackageLifecycleProposal](ModPackageLifecycleProposal.md) | accepted for planning | Modding, Application, Save | Discovery, explicit load order, lock file, validation и controlled restart UX |
| [LuaModuleOverrideProposal](LuaModuleOverrideProposal.md) | accepted for planning | Runtime, Modding, Headless | Замещение Lua-модуля пакетом с доступом к базе; заморозка таблиц экспорта |
| [CoreGameplayBoundaryProposal](CoreGameplayBoundaryProposal.md) | implemented | Architecture, Modding, Content | Правило ownership между framework core и gameplay packages: механизмы в `core`, семантика и контент в пакетах |
| [DesignerLuaAuthoringProposal](DesignerLuaAuthoringProposal.md) | implemented | Runtime, State, Authoring | Designer-facing Lua: дескриптор модуля с отложенной регистрацией, три вида property, `write_revision` и правило `fail()`, изоляция сырого состояния |
| [SimplifiedAuthoringSurfaceProposal](SimplifiedAuthoringSurfaceProposal.md) | implemented | Runtime, Authoring, Tooling | Окружение authoring-скрипта без `M.`, автообнаружение модулей, неявный успех команды, презентация без участия геймплея; переработка `rh` |
| [CommandValidatorAuthoringProposal](CommandValidatorAuthoringProposal.md) | accepted for planning | Runtime, Authoring, Modding | Designer-facing Validators как независимые read-only policies поверх существующего Command pipeline |
| [TextSystemLayerProposal](TextSystemLayerProposal.md) | implemented | Architecture, Modding, Content | Промежуточный слой `textsystem` между движком и игрой: локации, переходы, презентер; набор пакетов из данных |
| [MutationWindowTransactionalityProposal](MutationWindowTransactionalityProposal.md) | accepted for planning | Runtime, State, Commands | Журнал записей в окне мутации и откат канонического состояния при ошибке обработчика |
| [ContentEditorPluginProposal](ContentEditorPluginProposal.md) | accepted for planning | UI, Editor Tooling, Content | Плагин Unreal Editor как визуальный frontend поверх канонических `.json5`; `.uasset` не становится хранилищем |
| [CommonUIRuntimeIntegrationProposal](CommonUIRuntimeIntegrationProposal.md) | accepted for planning | UI, Presentation, Input | CommonUI для focus, input routing, activatable layers и Back без передачи gameplay authority |
| [ScreenAuthoringWorkflowProposal](ScreenAuthoringWorkflowProposal.md) | accepted for planning | UI, Editor Tooling | UMG Designer как canonical authoring surface и минимальный validator/editor workflow |
| [ImageResourceLookupOptimizationProposal](ImageResourceLookupOptimizationProposal.md) | implemented | UI, Resources, Engine | Immutable $O(1)$ lookup и однократная подготовка resolved brush |
| [ImageResourcePackagedDeploymentProposal](ImageResourcePackagedDeploymentProposal.md) | accepted for planning | UI, Resources, Build | Проверка `NonUFS` staging и единого resource root в packaged build |
| [ImageResourceDeferredLoadingProposal](ImageResourceDeferredLoadingProposal.md) | measurement required | UI, Resources, Operations | Условный async prepare/cache lifecycle после измерения startup и memory |
| [EntityAuthoringExtensionProposal](EntityAuthoringExtensionProposal.md) | implemented | Architecture, Runtime, Authoring | Декларативное расширение доменных сущностей через авторские прототипы `_ENV` |
| [UiCompositionAndScalingProposal](UiCompositionAndScalingProposal.md) | accepted for planning | UI, Presentation, Engine | Композиция UI: слои, оверлеи, модалки, вкладки, реконсиляция и масштабирование |
| [RHActorsLuaSimplificationProposal](RHActorsLuaSimplificationProposal.md) | accepted for planning | Runtime, Authoring, Gameplay | Контракты полей `field.*`, разделение инвариантов и предусловий, обобщённое создание инстансов |

## Рекомендуемый порядок

`PortableContentCoreProposal` выполнен. Оставшаяся очередь:

1. `ContentDiagnosticsAndToolingProposal` — реализованы CLI (`validate` с `--watch`, `inspect`, `describe`, `new`, `refs`, `rename`, `index`, `hash`), быстрая проверка Lua-модулей и интеграция с редактором; fuzzing, diff-отчёты и полноценный LSP остаются.
2. `LuaModuleOverrideProposal` — этап M1 (заморозка таблиц экспорта и разметка замещаемости) не зависит от пакетов и выполняется независимо; M2–M4 идут после `ModPackageLifecycleProposal`.
3. `CommandValidatorAuthoringProposal` — сначала ADR и contract update, затем Lua-only adapter и общие cross-host specs; runtime registry и C++ не расширяются без измеренной необходимости.
4. `ContentEditorPluginProposal` — предусловия закрыты планом [ContentEditorPrerequisites](../Plans/Archive/ContentEditorPrerequisites/README.md); блокировок не осталось. Декларативные экраны из `SimplifiedAuthoringSurfaceProposal` разблокированы тем же планом.
5. `ModPackageLifecycleProposal`.
6. `UiCompositionAndScalingProposal` — материализовано планом [UiComposition](../Plans/UiComposition/README.md): слои, оверлеи, вкладки, реконсиляция и масштабирование.
7. `CommonUIRuntimeIntegrationProposal` — после предыдущего: фокус и Back опираются на слои и вкладки.
8. `ScreenAuthoringWorkflowProposal`.

`RHActorsLuaSimplificationProposal` материализовано планом [RHActorsSimplification](../Plans/RHActorsSimplification/README.md) и выполняется независимо от Content/UI-треков. `MutationWindowTransactionalityProposal` выделено из него: контракты полей срабатывают в середине обработчика, а откат состояния при этом отсутствует.

`ImageResourceLookupOptimizationProposal` и `ImageResourcePackagedDeploymentProposal` могут выполняться независимо от основных Content/UI-треков. `ImageResourceDeferredLoadingProposal` начинается только после прохождения его measurement gate и обязательного обновления contracts/ADR.

UI-трек может выполняться независимо от Content-трека после стабилизации текущего Screen Template vertical slice. LSP, declarative trigger/effect DSL и подключение optional serialization library не входят в ближайший этап.
