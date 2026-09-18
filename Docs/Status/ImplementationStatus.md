---
title: Confirmed Contract Gaps
status: informative
version: 2.31
updated: 2026-09-18
depends_on:
  - ../README.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../Architecture/BuildAndTooling.md
  - ../UI/PresentationSnapshotAndEffects.md
  - ../ADR/0043-presentation-apply-boundary.md
  - ../ADR/0046-test-content-coupling-boundary.md
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
| `STATUS-026` | `known_nonconformance` | [Runtime Facade and Registries § Host-side freeze sequence](../Architecture/RuntimeFacadeAndRegistries.md#host-side-freeze-sequence): функции registry lifecycle не экспортируются в authoring/gameplay | `core:module.runtime.test_isolation` объявлен зависимостью `core:module.bootstrap.main`, поэтому модуль изоляции реестров загружается в каждой production-сессии, а не только в тестовом прогоне. Сама capability закрыта: `registry_lifecycle.take_isolation_handle()` отдаёт handle ровно один раз, его забирает `test_isolation` при загрузке, и любой последующий вызов получает `nil`, поэтому подменить запечатанный реестр из пакета нельзя (`UnauthorizedIsolation`). Расхождение в том, что модуль присутствует в игровом профиле вообще. **Условие закрытия:** у загрузчика появляется понятие профиля/видимости модулей, и `test_isolation` перестаёт входить в граф production-сессии; введение такого понятия требует отдельного ADR, а не правки внутри исправления. | `Scripts/bootstrap/manifest.lua` (`core:module.runtime.test_isolation` в dependencies `core:module.bootstrap.main`); `Scripts/bootstrap/registry_lifecycle.lua` (`take_isolation_handle`, `with_isolated_facade_slot`); спека `Tests/Lua/lifecycle/registry_sealing.lua` (`isolation_capability_is_single_use`). |

Автозапуск job'а снят 2026-09-15: при отсутствии runner'а GitHub держал job в очереди предельные 24 часа (`The job has exceeded the maximum execution time while awaiting a runner for 24h0m0s` — run `34672222339` на `035ac04`; run `34842198597` простоял 16 часов до отмены), а `timeout-minutes: 45` не тикает до старта job'а. Ручной `workflow_dispatch` не уменьшает расхождение и не является его обходом: он лишь перестаёт выдавать суточное ожидание за прогон. **Условие закрытия неизменно:** зарегистрированный self-hosted runner, названный успешный прогон job `Unreal GV2 Acceptance` на `origin` и возвращённый автозапуск на push. | `.github/workflows/linux-ci.yml` (`unreal-acceptance`, `runs-on: [self-hosted, linux, x64]`, `if: github.event_name == 'workflow_dispatch'`); `gh run view 34672222339`; `gh api repos/kkkingqz/ue5test/actions/runners`; перенесён при архивации плана C++ Foundation Closure из открытого пункта evidence задачи CFC-02. |
| `STATUS-028` | `known_nonconformance` | [Build and Tooling § Test content decoupling / Две категории теста](../Architecture/BuildAndTooling.md#test-content-decoupling), [ADR-0046](../ADR/0046-test-content-coupling-boundary.md): contract-тесты механики не вправе ссылаться на имена, ID сущностей или asset paths конкретных игровых пакетов | Contract-тесты ядра обязаны быть полностью отвязаны от игрового контента (`rh:`, `textsystem:`, `sample:`) и путей `/Game/...` с использованием только синтетических фикстур `core:`. Монолит `GV2RuntimeSubsystemTests.cpp` полностью очищен и декомпозирован (TSR-03..TSR-08), однако 8 тестовых файлов, созданных в предшествующих планах (DCA, DUC, UI), содержат 64 остаточных привязки к контенту (36 в `GV2PropertyConsumersTests.cpp`, 8 в `GV2Dca11InventoryTabsTests.cpp`, 7 в `GV2Duc10NestedChainTests.cpp`, 5 в `GV2UiPropertyHostTests.cpp`, 3 в `GV2DeclaredCompositeTests.cpp`, 2 в `GV2LayoutInvariantSourceTests.cpp`, 2 в `GV2UiCapabilityObservabilityTests.cpp`, 1 в `GV2WidgetSemanticFontSizeContractTests.cpp`), зафиксированных в ratchet baseline как временный остаток под `STATUS-028`. **Условие закрытия:** рефакторинг указанных 8 файлов с переводом на синтетические фикстуры `core:`, после чего `allowed_couplings` в baseline полностью обнуляется для contract-тестов. | `Tools/Testing/test_content_coupling_baseline.json` (`allowed_couplings` = 64, `residual_couplings_gap: "STATUS-028"`); `Tools/Testing/inventory_test_content_coupling.py`; CTest `test_content_coupling_contract`. |
| `STATUS-029` | `known_nonconformance` | [Build and Tooling § Universal acceptance assertion](../Architecture/BuildAndTooling.md): универсальное утверждение обязано называть actual enumerator; для enum/variant используется compiler/exhaustive dispatch | Множество queued `effect_id` перечислителя не имеет. `UGV2RuntimeSubsystem::HandlePresentationEffects` выбирает действие сравнением `EffectId` со строковыми литералами (`core:effect.rich_text_hover_open`, `core:effect.rich_text_hover_close`); нераспознанный accepted-эффект даёт `UE_LOG(Warning, "Unsupported presentation effect")` и молчаливое бездействие вместо отказа. `EPresentationEffectKind` с гейтом полноты существует, но перечисляет apply-виды (`Transparency`), а не queued-сигналы — это разные множества, и перечислитель есть только у одного. Сегодня расхождение ненаблюдаемо: очередь несёт ровно два сигнала одного продюсера, оба покрыты тестами поимённо. **Условие закрытия:** queued `effect_id` выводится из закрытого перечисления с гейтом полноты, и сигнал без ветки проваливает гейт. | `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp` (`HandlePresentationEffects`); `PEP-AF-06` в [Presentation Effect Pipeline Acceptance Findings](AuditFindings.md); `Source/GV2PresentationApply/Public/GV2PresentationApply/PresentationEffectApply.h` (`EPresentationEffectKind`, гейт полноты apply-видов). |

## Правило изменения

- Новый contract и полностью соответствующая реализация не создают строку.
- Частичная реализация создаёт или уточняет строку со ссылкой на точное правило и code/test evidence.
- Полное закрытие удаляет строку; история остаётся в commit и выполненном Plan summary.
- Предположение без проверки кода/tests сюда не добавляется.
