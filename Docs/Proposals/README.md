---
title: GV2 Implementation Proposals Index
status: normative
version: 4.1
updated: 2026-08-19
---

# Индекс предложений по реализации (Proposals)

Каталог содержит проработанные технические предложения по доработке, оптимизации и расширению подсистем GV2 до их прямого внедрения в код.

Proposal не изменяет нормативную архитектуру сам по себе. Если реализация меняет устойчивый инвариант, public contract или dependency direction, сначала создаётся ADR и синхронно обновляются затронутые contracts.

## Жизненный цикл предложения

Предложение живёт в этом каталоге ровно столько, сколько остаётся открытым вопросом. Как только вопрос закрыт — в любую сторону, — документ переезжает, и **каталог становится состоянием предложения**.

| `proposal_state` | Расположение | Значение |
|---|---|---|
| `accepted_for_planning` | `Proposals/` | Направление принято в backlog; до реализации действует текущий contract и набор `accepted` ADR |
| `measurement_required` | `Proposals/` | Реализация ещё не разрешена: сначала нужен указанный benchmark или evidence gate |
| `implemented` | [`Proposals/Archive/`](Archive/README.md) | Реализовано, нормативное поведение перенесено в contracts; документ сохраняется как rationale и implementation record |
| `rejected` | [`Proposals/Rejected/`](Rejected/README.md) | Рассмотрено и отклонено; документ сохраняется вместе с причиной отказа |

Связь состояния и каталога проверяется `Tools/Documentation/validate_docs.py` в обе стороны, вместе с обязательным `status: archived` для обоих исторических каталогов. Реализованное предложение, оставшееся в активном каталоге, продолжало бы выглядеть работой, которую надо сделать, поэтому перенос является частью завершения работы, а не уборкой после неё.

Отклонённое предложение не удаляется. Причина отказа — такой же результат размышления, как и принятое решение, и без неё тот же вопрос возвращается через несколько месяцев.

Трудоёмкость в документах относительная:

- **S** — локальное изменение без нового shared subsystem;
- **M** — несколько компонентов и обязательные integration tests;
- **L** — новый shared subsystem или несколько host integrations.

## Активные предложения

| Документ | Статус | Затронутые подсистемы | Описание |
|---|---|---|---|
| [ExternalProjectAdoptionProposal](ExternalProjectAdoptionProposal.md) | accepted for planning | Architecture, Dependencies | Матрица прямого использования, reference-only и отложенных внешних решений |
| [ContentDiagnosticsAndToolingProposal](ContentDiagnosticsAndToolingProposal.md) | accepted for planning | Content, CI, Tooling | Source spans, deterministic diagnostics, CLI validation, fuzzing и будущий LSP |
| [ModPackageLifecycleProposal](ModPackageLifecycleProposal.md) | accepted for planning | Modding, Application, Save | Discovery, explicit load order, lock file, validation и controlled restart UX |
| [LuaModuleOverrideProposal](LuaModuleOverrideProposal.md) | accepted for planning | Runtime, Modding, Headless | Замещение Lua-модуля пакетом с доступом к базе; заморозка таблиц экспорта |
| [MutationWindowTransactionalityProposal](MutationWindowTransactionalityProposal.md) | accepted for planning | Runtime, State, Commands | Журнал записей в окне мутации и откат канонического состояния при ошибке обработчика |
| [ContentEditorPluginProposal](ContentEditorPluginProposal.md) | accepted for planning | UI, Editor Tooling, Content | Плагин Unreal Editor как визуальный frontend поверх канонических `.json5`; `.uasset` не становится хранилищем |
| [UiCompositionAndScalingProposal](UiCompositionAndScalingProposal.md) | accepted for planning | UI, Presentation, Engine | Композиция UI: слои, оверлеи, модалки, вкладки, реконсиляция и масштабирование |
| [CommonUIRuntimeIntegrationProposal](CommonUIRuntimeIntegrationProposal.md) | accepted for planning | UI, Presentation, Input | CommonUI для focus, input routing, activatable layers и Back без передачи gameplay authority |
| [ScreenAuthoringWorkflowProposal](ScreenAuthoringWorkflowProposal.md) | accepted for planning | UI, Editor Tooling | UMG Designer как canonical authoring surface и минимальный validator/editor workflow |
| [ImageResourcePackagedDeploymentProposal](ImageResourcePackagedDeploymentProposal.md) | accepted for planning | UI, Resources, Build | Проверка `NonUFS` staging и единого resource root в packaged build |
| [ImageResourceDeferredLoadingProposal](ImageResourceDeferredLoadingProposal.md) | measurement required | UI, Resources, Operations | Условный async prepare/cache lifecycle после измерения startup и memory |

Реализованные предложения: [Archive](Archive/README.md). Отклонённые: [Rejected](Rejected/README.md).

## Рекомендуемый порядок

1. `ContentDiagnosticsAndToolingProposal` — реализованы CLI (`validate` с `--watch`, `inspect`, `describe`, `new`, `refs`, `rename`, `index`, `hash`), быстрая проверка Lua-модулей и интеграция с редактором; fuzzing, diff-отчёты и полноценный LSP остаются.
2. `LuaModuleOverrideProposal` — этап M1 (заморозка таблиц экспорта и разметка замещаемости) не зависит от пакетов и выполняется независимо; M2–M4 идут после `ModPackageLifecycleProposal`.
3. `CommandValidatorAuthoringProposal` — реализовано планом [CommandValidators](../Plans/Archive/CommandValidators/README.md).
4. `GameplayServiceAuthoringProposal` — реализовано планом [GameplayServices](../Plans/Archive/GameplayServices/README.md).
5. `ContentEditorPluginProposal` — предусловия закрыты планом [ContentEditorPrerequisites](../Plans/Archive/ContentEditorPrerequisites/README.md); блокировок не осталось.
6. `ModPackageLifecycleProposal`.
7. `UiCompositionAndScalingProposal` — материализовано планом [UiComposition](../Plans/UiComposition/README.md): слои, оверлеи, вкладки, реконсиляция и масштабирование.
8. `CommonUIRuntimeIntegrationProposal` — после предыдущего: фокус и Back опираются на слои и вкладки.
9. `ScreenAuthoringWorkflowProposal`.

`MutationWindowTransactionalityProposal` выделено из авторского слоя: контракты полей срабатывают в середине обработчика, а откат состояния при этом отсутствует.

`ImageResourcePackagedDeploymentProposal` может выполняться независимо от основных Content/UI-треков. `ImageResourceDeferredLoadingProposal` начинается только после прохождения его measurement gate и обязательного обновления contracts/ADR.

UI-трек может выполняться независимо от Content-трека после стабилизации текущего Screen Template vertical slice. LSP, declarative trigger/effect DSL и подключение optional serialization library не входят в ближайший этап.
