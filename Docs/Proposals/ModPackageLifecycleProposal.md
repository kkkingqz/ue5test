---
title: Mod Package Lifecycle Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-08-13
depends_on:
  - PortableContentCoreProposal.md
  - ContentDiagnosticsAndToolingProposal.md
  - ../Architecture/Modding.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../Architecture/CanonicalStateAndSave.md
decisions:
  - ../ADR/0006-repository-reload-and-session-pinning.md
  - ../ADR/0007-lua-module-environment.md
---

# Предложение по lifecycle и UX mod packages

> **Предлагает:** discovery модов, явный порядок загрузки, lock-файл и controlled restart.
> **Затрагивает:** [Modding](../Architecture/Modding.md), [GameDataRepository](../Architecture/GameDataRepositoryContract.md).
> **Не является нормативным:** до реализации действует текущий contract.

## Назначение и область

Предлагается единый application-level workflow обнаружения, проверки, упорядочивания и применения mod packages. UX использует понятные паттерны mod loaders, но content resolution остаётся собственным и подчиняется namespace, full override, provenance и controlled restart rules GV2.

## Владение и источники истины

- Mod manifest владеет identity, version, compatibility ranges, dependencies, content roots и optional Pak metadata.
- User configuration владеет желаемым enabled order.
- Package Resolver строит validated resolved order и generated `mods.lock.json5`.
- GameDataRepository владеет resolved immutable content snapshot.
- Save container фиксирует фактически использованные mods/order/versions/fingerprints.

`mods.lock.json5` является generated application data, а не authored mod manifest и не definition source. Он не редактируется mod Lua.

## Discovery и order

1. Просканировать только configured mod roots.
2. Прочитать manifest без исполнения Lua и без mount optional Pak.
3. Отклонить duplicate `mod_id`, invalid namespace, malformed manifest и unsupported API/schema ranges.
4. Применить explicit user enabled order.
5. Проверить required dependencies и cycles.
6. Использовать `load_after` только как editor suggestion; runtime не меняет order скрыто.
7. Построить deterministic package descriptors и fingerprints.
8. Передать descriptors в `GV2ContentCore` для full validation.

Filesystem enumeration order не является semantics. Diagnostics используют package-relative paths.

## Lock file

Минимальная generated запись:

```json5
{
  schema_version: 1,
  packages: [
    {
      mod_id: "weather_mod",
      version: "1.2.0",
      load_index: 1,
      fingerprint: "...",
      enabled: true,
    },
  ],
}
```

Lock file публикуется атомарно только после successful resolution. Absolute paths, credentials и mutable runtime handles в нём запрещены. Он помогает reproducible diagnostics и save preflight, но repository `content_hash` остаётся canonical identity content snapshot.

## User workflow

Минимальный набор операций:

- **Discover/Refresh** — обновляет candidate list без изменения active session.
- **Validate** — строит candidate package set/repository и показывает grouped diagnostics.
- **Enable/Disable/Reorder** — меняет pending configuration, но не current session.
- **Apply and Restart** — публикует valid candidate configuration и выполняет controlled session restart.
- **Open diagnostics** — группирует ошибки по mod, file, Stable ID и schema.

Invalid candidate не заменяет current lock/configuration и не влияет на active session. На cold start invalid enabled set открывает UE-native recovery surface с возможностью отключить проблемный mod и повторить validation.

## Trust boundary

- Mod Lua получает только фиксированный value-only Game API.
- Direct UObject, Blueprint reflection, filesystem, process API, native libraries и callbacks запрещены.
- Optional cooked Pak монтируется только после manifest/compatibility preflight и до repository build.
- Data-only mod не требует Pak и использует существующие Screen Templates, schemas и resources policies.
- Hot unmount и in-place Lua reload отсутствуют; изменение enabled set требует replacement session.

UE4SS используется только как reference для discover/enable/diagnostics UX. Project Alice используется как источник fixtures для dependencies, overrides, missing definitions и save compatibility; исходный runtime model не переносится.

## Failure и recovery

| Failure | Поведение |
|---|---|
| Manifest/discovery error | Mod недоступен для enable; остальные candidates продолжают отображаться |
| Dependency cycle/missing dependency | Candidate order invalid; active configuration сохраняется |
| Content/schema/reference error | Candidate repository не публикуется |
| Pak compatibility/mount error | Candidate session не создаётся; mount cleanup обязателен |
| Lua module bootstrap error | Candidate session уничтожается; recovery surface показывает mod/source |
| Missing mod при load save | Применяется existing opaque orphan/preflight policy; silent substitution запрещён |

## Этапы внедрения

1. Manifest DTO, discovery roots и deterministic package resolver.
2. Pending enabled order, validation flow и atomic lock file.
3. Recovery UI и `Apply and Restart` через existing lifecycle coordinator.
4. Save preflight/fingerprint diagnostics.
5. Optional Pak/Mod Kit support только при первом real asset mod.

## Польза, риски и трудоёмкость

- **Польза:** reproducible mod set, понятные ошибки до session restart и отсутствие hidden order.
- **Трудоёмкость:** **M** для data-only lifecycle; optional Pak support отдельно **L**.
- **Риск:** lock file становится вторым source of truth. Мера — он только generated result user configuration + resolver.
- **Риск:** unsafe mod expectations. Мера — явно документировать trusted gameplay code и закрытый UE/platform boundary.
- **Риск:** сложный in-place reload. Мера — только candidate build и controlled restart.

## Не входит в первый этап

- Mod marketplace, network downloads, signatures или hostile-code sandbox.
- Hot mount/unmount, live Lua patching или current-session repository swap.
- Native mod DLLs и прямой UObject access.
- Universal conflict merge editor; override остаётся полным по ID.

## Критерии приёмки

- Одинаковые manifests и user order дают одинаковые resolved descriptors, lock file и diagnostics.
- Enable/disable/reorder не меняет active session до successful `Apply and Restart`.
- Invalid candidate сохраняет current configuration, repository и session.
- Save metadata сравнивается с resolved lock/fingerprints до teardown current session.
- Tests покрывают duplicates, cycles, missing dependencies, full override, failed override, enable/disable, stale validation result и restart.
