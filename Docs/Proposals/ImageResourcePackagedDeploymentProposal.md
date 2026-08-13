---
title: Image Resource Packaged Deployment Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-08-13
depends_on:
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../UI/ImageResources.md
decisions:
  - ../ADR/0015-filesystem-discovered-image-resources.md
  - ../ADR/0016-png-suffix-image-metadata.md
---

# Предложение по проверке packaged deployment Image Resources

## Назначение и область

Предлагается проверить и зафиксировать работу filesystem-discovered Image Resources в staged Development/Shipping build. `NonUFS` staging уже реализован в `GV2.Build.cs`; ближайшая задача — verification фактического artifact layout и startup behavior, а не добавление второго staging mechanism.

## Оценка текущего состояния

| Область | Текущее состояние | Вывод |
|---|---|---|
| Staging | Файлы `Resources` добавляются как `RuntimeDependencies` с `StagedFileType.NonUFS` | Не дублировать |
| Root configuration | `ResourceRootDirectory` является project-relative | Сохранить |
| Runtime resolution | Используется `FPaths::ProjectDir() + ResourceRootDirectory` | Проверить на packaged artifact |
| Verification | Editor/automation fixtures существуют, packaged smoke test не зафиксирован | Добавить |

## Ownership и источник истины

- `GV2.Build.cs` владеет Unreal staging declaration.
- `UGV2ImageResourceCatalogSettings.ResourceRootDirectory` владеет logical project-relative root.
- Host-specific root resolution принадлежит `UGV2ImageResourceCatalogSettings`, а не catalog scanner или Widgets.
- `UGV2ImageResourceCatalog` получает уже разрешённый absolute root и не угадывает deployment layout.

## Инварианты

- Используется один configured root и один host-specific resolution rule.
- Missing root/file создаёт deterministic startup diagnostic.
- Несколько fallback directories и silent guessing запрещены.
- Absolute authored path не входит в config, Lua, definition или portable DTO.
- Active session не rescans filesystem; новое содержимое применяется через startup или controlled restart.
- Packaged и Editor используют одинаковую filename-to-`resource_id` grammar.

## Verification flow

1. Собрать staged Development или Shipping artifact.
2. Проверить присутствие expected files под configured relative root.
3. Запустить packaged executable с рабочим каталогом, отличным от project source directory.
4. Построить catalog обычным startup path.
5. Разрешить по одному fixture `fixed_aspect`, `tile` и `nine_slice`.
6. Повторить negative case с отсутствующим root или fixture.
7. Сохранить machine-readable test result и startup diagnostic.

Test не должен проходить за счёт случайного чтения source-tree `Resources` рядом с developer checkout.

## Возможное изменение root resolver

Если packaged smoke test подтверждает, что `FPaths::ProjectDir() + ResourceRootDirectory` не соответствует staged layout, добавляется ровно один resolver, выбирающий root по explicit host/build context.

Resolver обязан:

- возвращать normalized absolute path;
- проверять, что configured relative path не выходит за разрешённый application root;
- не перебирать `ProjectContentDir`, current working directory и arbitrary parent directories;
- выдавать diagnostic с logical root и build mode без раскрытия лишних host paths пользователю.

До подтверждённого failure текущего path policy код resolver-а не усложняется.

## Failure и recovery

| Failure | Поведение |
|---|---|
| Resources не попали в staged artifact | Packaging/test failure |
| Configured root отсутствует | Catalog build/startup failure; empty catalog не публикуется |
| Один required PNG отсутствует | Fixture/manifest expectation failure |
| PNG присутствует, но invalid | Общая candidate catalog validation failure |
| Path пытается выйти за application root | Configuration failure |

## Польза, риски и трудоёмкость

- **Польза:** подтверждённая работа authored filesystem resources вне Editor и source checkout.
- **Трудоёмкость:** **S–M**, зависит от существующей package automation.
- **Риск ложноположительного теста:** packaged executable запускается вне repository root.
- **Риск platform drift:** smoke test выполняется минимум на каждой shipping target platform в release pipeline.
- **Риск path fallback complexity:** resolver меняется только после воспроизводимого packaged failure.

## Не входит в предложение

- Изменение render modes или resource lookup structure.
- Перенос files в Pak/UFS, Unreal Data Registry или `.uasset` catalog.
- Runtime download, mod marketplace или hot mount.
- Deferred loading и texture memory policy.

## Критерии приёмки

- Packaged artifact содержит staged `Resources` в ожидаемом location.
- Packaged executable, запущенный вне source checkout, строит catalog через один configured root.
- Plain, tile и nine-slice fixtures успешно resolve.
- Missing root/fixture создаёт deterministic failure, а не silent empty catalog или поиск в другом directory.
- Build/test документация фиксирует проверенные target platforms и artifact command.
