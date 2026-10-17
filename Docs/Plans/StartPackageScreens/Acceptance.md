---
title: Start Package Screens Acceptance
status: active
version: 0.1
updated: 2026-10-17
depends_on:
  - README.md
  - ../../UI/ScreenTemplates.md
---

# Приёмка

> **Материализует:** этап M2 [плана](README.md): сквозную проверку новых экранов и сверку документов.

## SPS-09 — Приёмка экранов

- [ ] SPS-09 — Приёмка экранов

**Файлы:** `Source/GV2/Private/Tests/GV2ContentSmokeTests.cpp` или другой файл из `smoke_files`; `Docs/UI/ScreenTemplates.md`, `Docs/UI/WidgetRegistry.md`, `Docs/UI/SemanticInput.md`, `Docs/Authoring/PresentationAuthoringReference.md`, `Docs/Status/ScreenRequests.md`, `Docs/Proposals/CoreUiExtendedElementSetProposal.md`.

**Зависит от:** SPS-02…SPS-08; механизм режимов.

**Инвариант:** `AGENTS.md`, «Проверка стоит на пути, которым ходит продукт»: экран считается проверенным только показом через production-путь. Нарушение — экран, который применяет только тест.

**Не считается закрытием:**
- список экранов в тесте, написанный вручную;
- заявка SR, закрытая без ссылки на field contract.

**Done:**
- Каждая запись Screen Registry, добавленная планом, показывается через production-путь: экран локации — его presenter-ом, экраны режимов — route активного режима; перечислитель — resolved Screen Registry, сравнение с состоянием до плана.
- Каждая такая запись проходит шесть разрешений матрицы; перечислитель тот же.
- Новые тесты не добавляют привязок к контенту вне `smoke_files` (`test_content_coupling_contract`).
- `Docs/UI/ScreenTemplates.md` содержит field contract каждой новой schema; `Docs/UI/WidgetRegistry.md` — каждый новый элемент; перечислитель — schemas `ui_field`/`ui_value`, добавленные планом.
- Заявки SR-001…SR-005 закрыты со ссылками на field contracts.
- Строка `Tooltip` удалена из Core UI Extended Element Set как реализованная вместе с SR-001.
- `Docs/Authoring/PresentationAuthoringReference.md` показывает публикацию каждого нового экрана из Lua.

**Evidence:** отчёт `run_ue_tests.py` по всем экранам и разрешениям; `ctest`; `validate_docs.py`; результат обоих перечислений.
