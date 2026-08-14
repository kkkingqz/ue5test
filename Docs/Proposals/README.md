---
title: GV2 Implementation Proposals Index
status: normative
version: 1.2
updated: 2026-08-13
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
| [PortableContentCoreProposal](PortableContentCoreProposal.md) | accepted for planning | Content, Runtime, Headless | Общий portable pipeline `Packages → Definitions → Repository Snapshot → Runtime` |
| [ContentDiagnosticsAndToolingProposal](ContentDiagnosticsAndToolingProposal.md) | accepted for planning | Content, CI, Tooling | Source spans, deterministic diagnostics, CLI validation, fuzzing и будущий LSP |
| [ModPackageLifecycleProposal](ModPackageLifecycleProposal.md) | accepted for planning | Modding, Application, Save | Discovery, explicit load order, lock file, validation и controlled restart UX |
| [CommonUIRuntimeIntegrationProposal](CommonUIRuntimeIntegrationProposal.md) | accepted for planning | UI, Presentation, Input | CommonUI для focus, input routing, activatable layers и Back без передачи gameplay authority |
| [ScreenAuthoringWorkflowProposal](ScreenAuthoringWorkflowProposal.md) | accepted for planning | UI, Editor Tooling | UMG Designer как canonical authoring surface и минимальный validator/editor workflow |
| [ImageResourceLookupOptimizationProposal](ImageResourceLookupOptimizationProposal.md) | implemented | UI, Resources, Engine | Immutable $O(1)$ lookup и однократная подготовка resolved brush |
| [ImageResourcePackagedDeploymentProposal](ImageResourcePackagedDeploymentProposal.md) | accepted for planning | UI, Resources, Build | Проверка `NonUFS` staging и единого resource root в packaged build |
| [ImageResourceDeferredLoadingProposal](ImageResourceDeferredLoadingProposal.md) | measurement required | UI, Resources, Operations | Условный async prepare/cache lifecycle после измерения startup и memory |

## Рекомендуемый порядок

1. `PortableContentCoreProposal`.
2. `ContentDiagnosticsAndToolingProposal`.
3. `ModPackageLifecycleProposal`.
4. `CommonUIRuntimeIntegrationProposal`.
5. `ScreenAuthoringWorkflowProposal`.

`ImageResourceLookupOptimizationProposal` и `ImageResourcePackagedDeploymentProposal` могут выполняться независимо от основных Content/UI-треков. `ImageResourceDeferredLoadingProposal` начинается только после прохождения его measurement gate и обязательного обновления contracts/ADR.

UI-трек может выполняться независимо от Content-трека после стабилизации текущего Screen Template vertical slice. LSP, declarative trigger/effect DSL и подключение optional serialization library не входят в ближайший этап.
