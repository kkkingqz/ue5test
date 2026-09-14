---
title: Confirmed Contract Gaps
status: informative
version: 2.24
updated: 2026-09-14
depends_on:
  - ../README.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../UI/PresentationSnapshotAndEffects.md
  - ../ADR/0043-presentation-apply-boundary.md
---

# Подтверждённые расхождения contract и реализации

> **Показывает:** только проверенные незакрытые gaps между нормативным contract и текущим кодом/tests.
> **Не является нормативным:** целевое поведение задаёт linked owner contract; Plans и Proposals определяют будущую работу.
> **Обновляется:** gap добавляется при подтверждённом расхождении и удаляется тем же change set, который его полностью закрывает.

Отсутствие строки не доказывает полноту реализации. Реализованные возможности здесь не перечисляются: их evidence находится в коде и tests. Roadmap, приоритеты и идеи в этот документ не входят.

## Состояния

- `missing` — обязательная contract surface отсутствует.
- `partial` — существует только часть обязательного lifecycle/behavior.
- `known_nonconformance` — реализация существует, но наблюдаемо нарушает конкретное правило.

## Открытые gaps

| ID | Состояние | Нормативное требование | Точное расхождение | Evidence |
|---|---|---|---|---|
| `STATUS-002` | `missing` | [Presentation Snapshot and Effects § Effect](../UI/PresentationSnapshotAndEffects.md#effect), [§ ordering](../UI/PresentationSnapshotAndEffects.md#snapshoteffect-ordering) | UI document/reconciliation реализованы, но public one-shot effect DTO/queue/apply path, stale target handling и effect non-persistence tests отсутствуют. | В `Scripts/`, `Source/` и `Tests/` нет production `publish_effect`/effect queue consumer; `Scripts/authoring/presentation.lua` публикует только desired UI document. |
| `STATUS-003` | `missing` | [UI Document and Reconciliation § Reconciliation](../UI/UIDocumentAndReconciliation.md#reconciliation), [UI README](../UI/README.md) | Contract описывал optional enter/exit анимацию реконсиляции (removed/stale screen блокирует input "до завершения exit animation"), но ни `FGV2LayeredUiReconciler`, ни вызывающий `UGV2RuntimeSubsystem`/`FGV2SessionCoordinator` не содержат animation-гейтинга: detach/attach выполняются синхронно, без ожидания. | `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp` (`CommitReconcile` не запускает и не ждёт анимаций), `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`; PCC-11 (2026-08-31) обнаружено при аудите фактического порядка шагов `CommitReconcile` против контракта. |
| `STATUS-011` | `known_nonconformance` | [Universal UI Property Pipeline](../ADR/0040-universal-ui-property-pipeline.md), [UI Document and Reconciliation § Screen Fields](../UI/UIDocumentAndReconciliation.md) | `textsystem:schema.ui_field.location_scene.v1` объявляет `required: false` у **всех** полей верхнего уровня — `background_tile_resource_id`, `background_resource_id`, `context_text`, `characters`, `key`. Схема поля сцены не требует ничего, поэтому ревизия, в которой не пришло ни одного значения, проходит любую проверку обязательности этой схемы, и регрессия, при которой сцена перестаёт публиковаться, не отклоняется на границе. Для сравнения, `location_player_status.v1` и `location_commands.v1` объявляют обязательным по одному полю верхнего уровня. Обнаружено при закрытии проверки milestone M2 плана DeclaredCompositeAdoption: утверждение «каждое схемно-обязательное свойство пришло» для сцены оказалось пустым множеством. Обойдено в тесте вторым утверждением («композит получил хотя бы одно объявленное значение»), но это свойство теста, а не контракта. **Условие закрытия:** решено, какие поля сцены обязательны, и это записано в схеме; либо зафиксировано, почему сцена целиком необязательна, и чем тогда гарантируется её присутствие. | `GameData/textsystem/schemas/ui_field_location_scene_v1.schema.json5`; `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp` (`GV2.Runtime.Presentation.RhStartOpensLocationScreen`, метка `M2`); для контраста `ui_field_location_player_status_v1.schema.json5:10`, `ui_field_location_commands_v1.schema.json5:10`. |
| `STATUS-026` | `known_nonconformance` | [Runtime Facade and Registries § Host-side freeze sequence](../Architecture/RuntimeFacadeAndRegistries.md#host-side-freeze-sequence): функции registry lifecycle не экспортируются в authoring/gameplay | `core:module.runtime.test_isolation` объявлен зависимостью `core:module.bootstrap.main`, поэтому модуль изоляции реестров загружается в каждой production-сессии, а не только в тестовом прогоне. Сама capability закрыта: `registry_lifecycle.take_isolation_handle()` отдаёт handle ровно один раз, его забирает `test_isolation` при загрузке, и любой последующий вызов получает `nil`, поэтому подменить запечатанный реестр из пакета нельзя (`UnauthorizedIsolation`). Расхождение в том, что модуль присутствует в игровом профиле вообще. **Условие закрытия:** у загрузчика появляется понятие профиля/видимости модулей, и `test_isolation` перестаёт входить в граф production-сессии; введение такого понятия требует отдельного ADR, а не правки внутри исправления. | `Scripts/bootstrap/manifest.lua` (`core:module.runtime.test_isolation` в dependencies `core:module.bootstrap.main`); `Scripts/bootstrap/registry_lifecycle.lua` (`take_isolation_handle`, `with_isolated_facade_slot`); спека `Tests/Lua/lifecycle/registry_sealing.lua` (`isolation_capability_is_single_use`). |

## Правило изменения

- Новый contract и полностью соответствующая реализация не создают строку.
- Частичная реализация создаёт или уточняет строку со ссылкой на точное правило и code/test evidence.
- Полное закрытие удаляет строку; история остаётся в commit и выполненном Plan summary.
- Предположение без проверки кода/tests сюда не добавляется.
