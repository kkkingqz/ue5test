---
title: Cpp Foundation Closure Plan — Archive
status: archived
version: 1.0
updated: 2026-09-14
depends_on:
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../Architecture/CanonicalStateAndSave.md
  - ../../Architecture/BuildAndTooling.md
decisions:
  - ../../ADR/0020-cpp-scope-criterion.md
  - ../../ADR/0021-opaque-save-container.md
  - ../../ADR/0043-presentation-apply-boundary.md
  - ../../ADR/0044-session-replacement-and-registry-sealing.md
  - ../../ADR/0045-atomic-save-slot-generation-publication.md
---

# C++ Foundation Closure: итог выполнения

> **Материализует:** исторический итог фиксации C++-основы, на которой gameplay развивается в Lua. Документ не является источником правил или задач: поддержанная поверхность задана [Build and Tooling § Supported foundation baseline](../../Architecture/BuildAndTooling.md#supported-foundation-baseline), открытые расхождения — [Confirmed Contract Gaps](../../Status/ImplementationStatus.md).

## Цель и результат

**Цель.** Предыдущий план дал работающие механизмы — exact package set, snapshot, отдельный `GV2PresentationApply`, — но [аудит](../../Status/Archive/CppFoundationReadinessAudit2026-09-14.md) нашёл обходы вокруг них: materializer читал другой кэш, ambient getter выбирал старый snapshot, UE-host разрушал проекцию раньше решения coordinator, portable save существовал без продуктовых вызывающих, а зелёный runner допускал неисполненные тесты. Требовалось не усилить проверки поверх работающих helpers, а удалить обходные пути и связать lifecycle владельцев.

**Результат.** Приёмка стала fail-closed: actual set выводится из UE discovery, идентичность бинарника — из рантайма загруженного модуля, неполный или несовпадающий отчёт краснеет. Session ownership замкнут: один snapshot, compile-to-value Screen Registry, обязательная фаза sealing реестров, две границы replacement с разной семантикой отказа. Canonical state и его сборка принадлежат Lua целиком, seed передан как отдельный deterministic session input. Save/load доведён до продукта: immutable generations, один storage-owned head, atomic publish, safe-point сохранение из UI-команды и загрузка через тот же replacement. Сцена стала обязательным контрактом данных, сквозной gameplay-срез подтверждён без новой native логики, поддержанная поверхность зафиксирована явно.

**Границы результата.** План принят для Linux Editor/Development. Shipping/cook/package, другие ОС, GPU/rendered visual matrix, power-loss durability и длительная эксплуатация в baseline не входят. One-shot effects и enter/exit animations остались открытыми (`STATUS-002`, `STATUS-003`); одноразовая registry isolation capability — `STATUS-026`; неисполняемый на origin job UE-приёмки — `STATUS-027`.

## Этапы

| Этап | Итог |
|---|---|
| M0 — Достоверная приёмка | Единый fail-closed runner и report validator, изоляция automation fixtures, fail-closed разбор `Build.cs` и канонические numbers/hash-поля. Приёмка перепривязана к текущему коду после того, как собственные поставки этапа были переписаны; центральная поставка проверена тем, что отвергла 158/158 зелёных на устаревшем бинарнике. |
| M1 — Session ownership | Snapshot стал единственным источником UI-схем, resolved Screen Registry — независимым значением, prepared UI получил явный GC ownership, sealing реестров — обязательной фазой, candidate передаётся явно, lifecycle requests и teardown замкнуты, canonical state и seed переданы Lua. Принято по механизмам: множества выводятся перечислителями, каждый вывод проверен мутацией. |
| M2 — Save/load в игре | Предыдущее поколение слота сохраняется, storage и safe-point save подключены к UE-host, захваченные bytes загружаются через единый replacement. Этап принят со второго захода: первая версия проверялась структурно, и четыре теста зеленели на тех самых регрессиях, ради которых были написаны. |
| M3 — Lua baseline | Присутствие сцены стало проверяемым контрактом данных, gameplay-срез подтверждён без новой native логики, поддержанная C++/Lua-поверхность зафиксирована. Финальная приёмка исполнила все 16 обязательных targeted mutations в disposable checkout и исправила четыре дефекта, которые прежняя зелёная матрица не ловила. |

## Задачи

| ID | Исходное название |
|---|---|
| `CFC-01` | Согласовать границы и проверяемые outcomes |
| `CFC-02` | Сделать UE-приёмку единой и fail-closed |
| `CFC-02A` | Изолировать lifetime и mutable настройки automation fixtures |
| `CFC-03` | Закрыть неполный inventory графа сборки |
| `CFC-03A` | Замкнуть канонические numbers и validation hash-полей |
| `CFC-04` | Сделать snapshot единственным источником UI-схем |
| `CFC-04A` | Сделать resolved Screen Registry независимым значением snapshot |
| `CFC-04B` | Зафиксировать GC ownership prepared UI и границу Game Thread |
| `CFC-05` | Сделать registry sealing обязательной фазой запуска |
| `CFC-05A` | Передать сборку canonical state целиком Lua |
| `CFC-06` | Передавать candidate явно и объединить publication с UE projection |
| `CFC-07` | Завершить lifecycle requests, отмену и teardown |
| `CFC-07A` | Передать seed через единый deterministic session input |
| `CFC-08` | Сохранять предыдущее поколение opaque slot |
| `CFC-09` | Подключить storage и safe-point save к UE-host |
| `CFC-10` | Загрузить захваченные bytes через единый replacement |
| `CFC-11` | Сделать присутствие сцены проверяемым контрактом данных |
| `CFC-12` | Подтвердить gameplay-срез без новой native логики |
| `CFC-13` | Зафиксировать поддержанную C++/Lua-поверхность |

## Верификация

- Зафиксированная code/evidence revision — `02cb996b4b905f383b724aa099b1b9324cebd2f5`, отдельный чистый worktree.
- Release CTest `128/128`; ASan+UBSan CTest `128/128` без sanitizer diagnostics.
- UBT `GV2Editor Linux Development` — `Result: Succeeded`.
- Fresh-process UE inventory `173/173`, failed/skipped `0`, `source_diff_hash=clean`, build fingerprint `9f94aa000f1f1ebafdbb804dbf92f5ff83af81aa6d0270a3bf1ff238311785bd`.
- `gv2-headless --self-test`, проверка 50 Lua-модулей, content validate/coverage и documentation validation — success.
- Все 16 обязательных targeted mutations внесены в disposable checkout и отвергнуты по ожидаемой причине; отдельно исполнены A → failed B → successful B с разными package closures.
- Полный UE report содержит 230 warnings и warning-free evidence не является. Удалённый CI за эту ревизию не засчитан — см. `STATUS-027`.

## Что план изменил в правилах приёмки

Универсальное утверждение приёмки обязано называть actual enumerator, независимый oracle и production path. Для UE-прогона actual set выводится discovery текущего build, а completed records обязаны совпасть с ним один к одному; для enum используется exhaustive dispatch. Идентичность бинарника происходит из рантайма, а не из окружения раннера, поэтому зелёный прогон на устаревшей сборке не засчитывается. Правило перенесено в [Build and Tooling](../../Architecture/BuildAndTooling.md#supported-foundation-baseline) и живёт дальше без плана.

## Актуальные owner contracts

- [Bootstrap and Session Lifecycle](../../Architecture/BootstrapAndSessionLifecycle.md)
- [Runtime Facade and Registries](../../Architecture/RuntimeFacadeAndRegistries.md)
- [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md)
- [Build and Tooling](../../Architecture/BuildAndTooling.md)
- [ADR-0020: C++ Scope Criterion](../../ADR/0020-cpp-scope-criterion.md)
- [ADR-0021: Opaque Save Container](../../ADR/0021-opaque-save-container.md)
- [ADR-0043: Presentation Apply Boundary](../../ADR/0043-presentation-apply-boundary.md)
- [ADR-0044: Session Replacement and Registry Sealing](../../ADR/0044-session-replacement-and-registry-sealing.md)
- [ADR-0045: Atomic Save Slot Generation Publication](../../ADR/0045-atomic-save-slot-generation-publication.md)

## Source record

`source_commit`: `d261508700adecc3a8baff8c5bd8f7193125fa9e` ([browse commit](https://github.com/kkkingqz/ue5test/commit/d261508700adecc3a8baff8c5bd8f7193125fa9e)).

Перед удалением все пять исходных paths проверены через `git cat-file -e <source_commit>:<path>`, а `SaveAndGameplay.md` восстановлен через `git show` и побайтно сопоставлен с рабочим файлом. Полные задачи с полями «Инвариант», «Не считается закрытием», «Done» и «Evidence», записи приёмки каждого этапа и таблица соответствия задач и change set доступны по этому commit.
