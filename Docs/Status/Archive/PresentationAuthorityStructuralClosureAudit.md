---
title: Presentation Authority Structural Closure Audit — Archive
status: archived
version: 1.0
updated: 2026-09-11
depends_on:
  - ../ImplementationStatus.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../UI/UIDocumentAndReconciliation.md
decisions:
  - ../../ADR/0042-presentation-authority-and-publication.md
  - ../../ADR/0043-presentation-apply-boundary.md
---

# Presentation Authority Structural Closure: итог аудита

> **Показывает:** итог повторного ревью presentation authority, последующей независимой сверки и устранения найденных расхождений; документ не является источником правил или задач.

## Охват и метод

Раунд начал с повторного анализа authority, package identity, session snapshot, Screen Registry, resources и verification architecture после `PresentationAuthorityHardening`. Каждая находка сверялась с production-кодом и закрывалась механизмом, чей actual set выводится компилятором, рефлексией, декларациями или production call graph. Финальная сверка повторила portable, Headless и полный Unreal-набор и добавила две viewport-находки, обнаруженные непосредственно при проверке игры в PIE.

## Счёт

| Категория | Найдено | Устранено | Перенесено в status gaps | Отклонено |
|---|---:|---:|---:|---:|
| Presentation authority и session ownership | 4 | 4 | 0 | 0 |
| Package/resource identity и isolation | 2 | 2 | 0 | 0 |
| Verification architecture | 1 | 1 | 0 | 0 |
| Viewport lifecycle и geometry | 2 | 2 | 0 | 0 |
| **Всего** | **9** | **9** | **0** | **0** |

## Outcomes

| ID и исходная формулировка | Исход |
|---|---|
| `PAH-R1` — P1 — Theme повторяет defect класса `STATUS-012` | *(Закрыто задачами PSC-10A и PSC-10B.)* Theme разрешается в Prepare; concrete style/scale payload применяется общей транзакцией, runtime accessors удалены и запрещены structural gates. |
| `PAH-R2` — P1 — принятого `FSessionContentSnapshot` фактически нет | *(Закрыто задачами PSC-04, PSC-05 и PSC-06.)* Coordinator атомарно публикует один immutable snapshot, а candidate и прежняя active session имеют независимые lifetimes. |
| `PAH-R3` — P1 — Screen Registry использует другой package authority | *(Закрыто задачами PSC-02, PSC-04 и PSC-08.)* Repository, Lua и presentation builders получают один resolved package set; screen resolution использует только snapshot-backed PrepareContext. |
| `PAH-R4` — P1/P2 — nested Tabs обходят session Registry authority | *(Закрыто задачей PSC-08.)* Embedded screen обязан пройти через `PrepareContext.ResolveScreen`; configured Registry и generic class fallback удалены. |
| `PAH-R5` — P1/P2 — disabled resource package влияет на session bootstrap | *(Закрыто задачей PSC-07.)* Resource builder сначала ограничивает обход exact enabled roots; повреждённый отключённый пакет не читается. |
| `PAH-R6` — P2 — `ue_content_roots` отсутствует в package fingerprint | *(Закрыто задачей PSC-03.)* Fingerprint учитывает canonical hash полного manifest, не перенося UE-семантику в portable descriptor. |
| `PAH-R7` — P1/P2 — verification gates перечисляют authorities вручную | *(Закрыто задачей PSC-13.)* Первичные гарантии основаны на compiler-exhaustive видах, field inventories и автоматически выведенных module/source graphs; token scans оставлены secondary defense. |
| `PSC-AF-01` — P1 — committed presentation не реагировала на resize viewport | *(Закрыто задачей PSC-14.)* Runtime обрабатывает production viewport-resize event и обновляет viewport-derived значения через единственную Apply façade, сохраняя Widget identity и UI-local state. |
| `PSC-AF-02` — P1 — presentation root не заполняет PIE viewport | *(Закрыто задачей PSC-14.)* Keyed Commit и rollback повторно применяют GameShell Fill policy к свежим slots; production session и все canonical layers проверены на независимой матрице размеров. |

Ни одно расхождение этого раунда не переносилось в [Implementation Status](../ImplementationStatus.md). Остальные строки status-документа находились вне области проверки и не менялись.

## Актуальные owner contracts

- [Bootstrap and Session Lifecycle](../../Architecture/BootstrapAndSessionLifecycle.md)
- [UI Document and Reconciliation](../../UI/UIDocumentAndReconciliation.md)
- [Blueprint Screen Template Contract](../../UI/ScreenTemplates.md)
- [ADR-0042: Presentation Authority and Publication](../../ADR/0042-presentation-authority-and-publication.md)
- [ADR-0043: Presentation Apply Boundary](../../ADR/0043-presentation-apply-boundary.md)

## Source record

`source_commit`: `89cbbcb557e671635115a478bf9f30f1364a74e1` ([browse commit](https://github.com/kkkingqz/ue5test/commit/89cbbcb557e671635115a478bf9f30f1364a74e1)).

Перед удалением `Docs/Status/AuditFindings.md` проверен через `git cat-file -e <source_commit>:Docs/Status/AuditFindings.md`, восстановлен через `git show` и побайтно сопоставлен с рабочим файлом.
