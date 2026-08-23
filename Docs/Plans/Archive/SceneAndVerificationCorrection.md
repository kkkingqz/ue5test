---
title: SceneAndVerificationCorrection Archive Summary
status: archived
version: 1.0
updated: 2026-08-23
---

# SceneAndVerificationCorrection: итог выполнения

> **Материализует:** исторический итог выполненного плана; документ не является источником правил или задач.

## Цель и результат

**Цель:** устранить findings [Plan Audit Findings](../../Status/AuditFindings.md), затрагивающие несколько слоёв сразу или требующие построения средства измерения там, где его нет — персонаж сцены через границу Lua → C++ → ассет, измерение фактической геометрии/font size/brush вместо желаемого размера, отклонение handle неактивной вкладки, судьба аварийных экранов, откат композита, канонический разбор Stable ID в редакторе, два отложенных архитектурных решения.

**Результат:** персонаж, объявленный в определении экрана, доходит до экрана через единый контракт `characters` в `textsystem:schema.ui_field.location_scene.v1`; матрица разрешений измеряет выделенную (arranged) геометрию, а не `GetDesiredSize()`; вакуумный тест перехода снят; handle неактивной вкладки отклоняется Semantic Input как `StaleBindingHandle`; ложное утверждение о существовании трёх аварийных экранов заменено документированной реальной поверхностью отказа `UGV2RecoveryScreenWidget`; откат композита при отказе ребёнка покрыт тестом; Definition Browser переведён на канонический `FStableId::Parse`; оба отложенных решения (форма `FGV2ScreenFieldValue`, композиция источников презентации) приняты как мотивированный отказ с наблюдаемым условием пересмотра.

Дополнительно задача SVC-12 сверила итоговый DoD [CriticalCorrectiveHardening](CriticalCorrectiveHardening.md) (был не отмечен, хотя все пять этапов завершены с evidence), сузила overclaim «экран практически проверяет базовый набор» в [LocationScreen](LocationScreen.md) до фактически проверенного, закрыла 13 findings в [AuditFindings](../../Status/AuditFindings.md) и перенесла в архив четыре завершённых плана ([CriticalCorrectiveHardening](CriticalCorrectiveHardening.md), [LocationScreen](LocationScreen.md), [UiFoundationHardening](UiFoundationHardening.md), [ContentEditorHardening](ContentEditorHardening.md)).

## Этапы и задачи

### M1 — Scene Character

Персонаж, объявленный в определении экрана, доходит до экрана.

- `SVC-01` — Контракт поля сцены описывает персонажей
- `SVC-02` — Хост репитера и привязка персонажа в ассете
- `SVC-03` — Согласовать имя и форму на границе
- `SVC-04` — Тест проходит через границу

### M2 — Measured Verification

Проверки измеряют заявленную величину, вакуумных тестов в наборе не остаётся.

- `SVC-05` — Матрица разрешений измеряет выделенную геометрию
- `SVC-06` — Снять вакуумный тест перехода
- `SVC-07` — Handle неактивной вкладки отклоняется
- `SVC-08` — Аварийные экраны: реализовать либо снять утверждение
- `SVC-09` — Тест отката композита

### M3 — Editor and Decisions

Редактор перестаёт содержать вторую грамматику Stable ID, два отложенных решения приняты, проверка планов закрыта.

- `SVC-10` — Дерево браузера через канонический парсер и инструментовка
- `SVC-11` — Два решения: форма значения поля и композиция источников ([ADR-0038](../../ADR/0038-screen-field-value-flat-struct.md), [ADR-0039](../../ADR/0039-presentation-source-singleton.md))
- `SVC-12` — Закрытие проверки

## Проверка

Полный регрессионный прогон на итоговом коммите: 66/66 портативных CTest, 81/81 UE automation тестов, `gv2-headless --self-test`/`--check-scripts`, `validate_docs.py` — без ошибок.

## Актуальные нормативные источники

- [ScreenTemplates](../../UI/ScreenTemplates.md)
- [SemanticInput](../../UI/SemanticInput.md)
- [UI/README](../../UI/README.md)
- [StableIDSpecification](../../Architecture/StableIDSpecification.md)
- [ADR-0038](../../ADR/0038-screen-field-value-flat-struct.md), [ADR-0039](../../ADR/0039-presentation-source-singleton.md)

## Полная история

`source_commit`: [68fdb9c3b8d8f3265c4926754390702b44efe687](https://github.com/kkkingqz/ue5test/commit/68fdb9c3b8d8f3265c4926754390702b44efe687)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/68fdb9c3b8d8f3265c4926754390702b44efe687/Docs/Plans/SceneAndVerificationCorrection) содержит исходные task-файлы, acceptance criteria и evidence.
