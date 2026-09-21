# GV2

GV2 — модульная игровая система на базе Unreal Engine 5 и Lua.

## Навигация по проекту

- **Документация и архитектурные контракты:** [`Docs/README.md`](Docs/README.md)
- **Инструкции и правила для AI-агентов:** [`AGENTS.md`](AGENTS.md) и [`GEMINI.md`](GEMINI.md)
- **Интеграция с Unreal Editor через MCP (Model Context Protocol):** [`Tools/MCP/README.md`](Tools/MCP/README.md)
- **Поднять проект на новой машине:** [`WORKSTATION-SETUP.md`](WORKSTATION-SETUP.md)

## Быстрый старт с инструментами MCP

Для взаимодействия с запущенным Unreal Editor по протоколу MCP используются скрипты из каталога [`Tools/MCP/`](Tools/MCP/README.md):

- **Запуск автотестов в редакторе:**
  ```bash
  python3 Tools/MCP/run_ue_tests.py
  ```
- **Компиляция и сохранение виджета:**
  ```bash
  python3 Tools/MCP/compile_and_save_assets.py --path "/Game/UI/Widgets/WBP_Button"
  ```
