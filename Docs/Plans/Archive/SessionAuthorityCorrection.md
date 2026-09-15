---
title: Session Authority Correction Implementation Plan — Archive
status: archived
version: 1.0
updated: 2026-09-16
depends_on:
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../Architecture/BuildAndTooling.md
  - ../../UI/ImageResources.md
decisions:
  - ../../ADR/0042-presentation-authority-and-publication.md
  - ../../ADR/0043-presentation-apply-boundary.md
  - ../../ADR/0044-session-replacement-and-registry-sealing.md
---

# Session Authority Correction: архив плана

> **Материализует:** исторический итог исправления session/presentation authority после повторной приёмки C++ foundation. Актуальные правила задают owner contracts и accepted ADR.

**Цель.** Устранить пути в обход уже принятых механизмов: разрушение Ready-сессии до transition policy, повторное чтение manifest, непроверяемые discovery-обоснования, mutable Theme вне session identity, нетипизированный terminal failure и безграничную историю operation outcomes.

**Результат.** Публичный session entry всегда проходит coordinator; resolved package set захватывает manifest-факты однократно; discovery callers выводятся из production source; Theme и effective fallback входят в immutable presentation identity; failure writer принимает только typed token; terminal outcomes имеют измеренную границу, детерминированное вытеснение и отличимый статус `Evicted`.

## Этапы

| Milestone | Результат |
|---|---|
| M0 — Вход и факты | Отказ candidate не разрушает Ready-сессию; manifest читается одним typed parse; фактические discovery callers сверяются механически |
| M1 — Идентичность содержимого | Effective Theme с fallback и нормализованный image content входят в `PresentationHash`/`SessionContentId`; snapshot не зависит от последующей мутации authoring assets |
| M2 — Lifecycle API | `Failed` требует закрытый typed fault, неизвестная runtime-причина сохраняется в `CauseCode`; история operation outcomes ограничена измеренным лимитом `18` |

## Задачи

| Задача | Исходное название |
|---|---|
| `SAC-01` | Убрать вторую валидацию из публичного входа |
| `SAC-02` | Захватить `ue_content_roots` в resolved package set |
| `SAC-03` | Сделать обоснование discovery-маркера проверяемым |
| `SAC-04` | Компилировать Theme в значение и внести её в идентичность |
| `SAC-05` | Связать typed fault с terminal outcome |
| `SAC-06` | Ограничить историю операций и убрать мёртвую очистку |

## Верификация приёмки

- Portable build завершён успешно; полный CTest — `134/134`.
- UBT `GV2Editor Linux Development` завершён успешно.
- Fresh-process UE Automation — `195/195`, failed/skipped `0`; отдельные operation measurement/retention tests — `2/2`.
- Production trace `session_lifecycle_v1` наблюдает IDs `1…6`: `Completed = 3`, `Failed = 3`; policy трёх измеренных окон выводит `DefaultMaxRetainedOutcomes = 18`.
- Documentation, transition ownership и measurement positive/negative gates прошли; tracked measurement запускается только в read-only `--check`.

## Актуальные owner contracts

- [Bootstrap and Session Lifecycle](../../Architecture/BootstrapAndSessionLifecycle.md)
- [Runtime Facade and Registries](../../Architecture/RuntimeFacadeAndRegistries.md)
- [Build and Tooling](../../Architecture/BuildAndTooling.md)
- [Image Resources](../../UI/ImageResources.md)

## Source record

`source_commit`: `36ac31b442648f3a3612df57a0ce8b24c696cb56` ([browse commit](https://github.com/kkkingqz/ue5test/commit/36ac31b442648f3a3612df57a0ce8b24c696cb56), [полный исходный план](https://github.com/kkkingqz/ue5test/blob/36ac31b442648f3a3612df57a0ce8b24c696cb56/Docs/Plans/SessionAuthorityCorrection/README.md)).

Перед удалением каждый исходный path проверен через `git cat-file -e <source_commit>:<path>`, representative `README.md` восстановлен через `git show`; SHA-256 восстановленного и рабочего файлов совпал (`0f5b4bc6f808df11df7224785cf5266f34c60bd1b00c5b2939d6bab216dcac5f`).
