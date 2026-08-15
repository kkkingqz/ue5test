# Интеграция с редакторами кода

В этом каталоге и в каталоге `.vscode/` репозитория находятся вспомогательные конфигурации для авторов контента и разработчиков.

> [!NOTE]
> Конфигурация редактора является **полностью опциональной**. Сборка проекта через CMake, тесты CTest, запуск `gv2-headless` и сборка Unreal Engine не зависят от наличия этих файлов.

---

## 1. Возможности интеграции

### Живая проверка контента на лету (`validate --watch`)
- Задача `GV2: Watch Content (GameData/core)` в `.vscode/tasks.json` запускает фоновый процесс наблюдения за изменениями файлов пакета:
  ```bash
  ./build/Tools/Content/gv2-content validate GameData/core --watch
  ```
- Встроенный `problemMatcher` автоматически парсит диагностику `gv2-content` (`<file>:<line>:<col>: error: <message>`) и выводит ошибки и предупреждения непосредственно в панель **Problems** редактора и на соответствующие строки файлов `.json5`.

### Автодополнение идентификаторов и подсказки (`gv2-content index`)
- Скрипт `Tools/Editor/generate_vscode_snippets.py` читает канонический индекс пакета `gv2-content index <package-root> --format=json` и генерирует файл фрагментов автодополнения `.vscode/gv2-content.code-snippets`.
- `.vscode/tasks.json` и `.vscode/settings.json` хранятся в репозитории; сгенерированный `.vscode/gv2-content.code-snippets` — нет, он воспроизводится скриптом и остаётся в `.gitignore`.
- Автору доступны префиксы для вставки полных и относительных Stable ID всех активных определений (`core:location.city.market`, `core:item.weapon.iron_sword` и т.д.) с контекстным описанием категории (`[location]`, `[item]`, `[text]`, `[actor]`).
- Для обновления сниппетов после добавления новых определений:
  ```bash
  python3 Tools/Editor/generate_vscode_snippets.py ./build/Tools/Content/gv2-content GameData/core --output .vscode/gv2-content.code-snippets
  ```
  (или задача `GV2: Update Content Snippets & Index` в VS Code).

### Проверка Lua-скриптов
- Задача `GV2: Check Lua Scripts` запускает статический синтаксический чекер:
  ```bash
  ./build/Headless/gv2-headless --check-scripts
  ```

---

## 2. Подключение в других редакторах

Инструменты `gv2-content` разработаны независимыми от конкретной IDE и могут быть подключены в любой редактор:

- **Формат ошибок для Problem Matcher / Error Regex**:
  `^([^:\s]+):(\d+):(\d+):\s+(error|warning):\s+(.*)$`
  (группы: 1 = файл, 2 = строка, 3 = колонка, 4 = уровень серьезности, 5 = сообщение).
- **Машиночитаемый JSON**:
  Все команды (`validate`, `index`, `describe`, `new`, `refs`, `rename`, `hash`) поддерживают флаг `--format=json` для интеграции с внешними инструментами, LSP и плагинами.
