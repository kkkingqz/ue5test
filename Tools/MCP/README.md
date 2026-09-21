# Unreal Engine MCP Integration Tools & Guidelines for AI Agents

В этом каталоге собраны инструменты, клиентские библиотеки и инструкции для взаимодействия AI-агентов с запущенным процессом Unreal Editor через **Model Context Protocol (MCP)**.

---

## 1. Архитектура и конфигурация

Unreal Editor предоставляет HTTP/SSE эндпоинт MCP через плагин `ModelContextProtocol`.
- **URL по умолчанию:** `http://127.0.0.1:8000/mcp`
- **Конфигурация репозитория:** `.mcp.json` в корне проекта
- **Протокол:** JSON-RPC 2.0 (метод `initialize`, затем вызовы `tools/call` для `list_toolsets`, `describe_toolset`, `call_tool`)

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "type": "http",
      "url": "http://127.0.0.1:8000/mcp"
    }
  }
}
```

---

## 2. Обязательные правила для AI-агента

1. **Создание и изменение ассетов (`.uasset`):**
   - AI-агент **обязан** создавать и изменять `.uasset` (Widget Blueprint, Data Asset, UI Theme, Texture) исключительно через Unreal Editor API (`unreal-mcp`), а не прямой записью бинарных файлов.
   - Если редактор не запущен, агент проверяет статус процесса (`pgrep UnrealEditor`) и сообщает пользователю о необходимости запуска редактора.

2. **Компиляция и сохранение:**
   - После изменения любого Blueprint/Widget Blueprint агент обязан скомпилировать его через `editor_toolset.toolsets.blueprint.BlueprintTools.compile_blueprint` и сохранить ассет через `editor_toolset.toolsets.asset.AssetTools.save_assets`.

3. **Проверка автоматизационных тестов:**
   - Запуск полного набора автотестов движка в работающем редакторе выполняется через `AutomationTestToolset.AutomationTestToolset` (или утилиту `Tools/MCP/run_ue_tests.py`).

---

## 3. Содержимое каталога `Tools/MCP/`

| Файл | Назначение |
|---|---|
| [`mcp_client.py`](mcp_client.py) | Автономная клиентская библиотека `UnrealMcpClient` для подключения к MCP серверу редактора. |
| [`run_ue_tests.py`](run_ue_tests.py) | CLI-утилита для запуска автоматизационных тестов в запущенном Unreal Editor с фильтрацией и отчетом. |
| [`compile_and_save_assets.py`](compile_and_save_assets.py) | CLI-утилита для пакетной компиляции и сохранения ассетов и виджетов. |
| [`README.md`](README.md) | Данное руководство. |

---

## 4. Справочник основных MCP-тулсетов

### 4.1 `AutomationTestToolset.AutomationTestToolset`
Управление запуском и сбором результатов тестов автоматизации Unreal Engine:
- `DiscoverTests(bForceRediscover: bool)` — инициализация discovery воркеров и загрузка списка тестов.
- `ListTests(nameFilter: str, tagFilter: str, limit: int)` — поиск тестов по маске (например `nameFilter="GV2"`).
- `RunTestsByFilter(filterExpression: str)` — быстрый запуск тестов по выражению (например `"StartsWith:GV2"`).
- `RunTests(testNames: list[str])` — запуск списка тестов по точным путям.
- `GetTestResults()` — получение детального отчета по последнему прогону (кол-во pass/fail, список ошибок, ворнингов и длительность).
- `GetTestStatus()` — легкий снапшот текущего состояния контроллера автоматизации.

### 4.2 `editor_toolset.toolsets.asset.AssetTools`
Операции с ассетами:
- `find_assets(folder_path: str, name: str, recursive: bool)` — поиск ассетов по пути и имени.
- `save_assets(asset_paths: list[str])` — сохранение измененных ассетов на диск.
- `duplicate_asset(source_path: str, destination_path: str)` — дублирование ассета.
- `delete_asset(asset_path: str)` — удаление ассета.

### 4.3 `editor_toolset.toolsets.object.ObjectTools`
Чтение и запись свойств UObject и DataAsset:
- `get_properties(instance: dict, properties: list[str])` — чтение свойств объекта (передается `refPath` или `objPath`).
- `set_properties(instance: dict, values: str)` — запись свойств в формате JSON-строки.
- `list_properties(instance: dict)` — перечисление доступных свойств объекта.

### 4.4 `editor_toolset.toolsets.blueprint.BlueprintTools`
Работа с графами и компиляцией Blueprint:
- `compile_blueprint(blueprint: dict)` — компиляция Blueprint (`{'refPath': '/Game/.../WBP_Name.WBP_Name'}`).
- `get_graph(blueprint: dict, graph_name: str)` — получение графа функций или событий.
- `list_functions(blueprint: dict)` — список функций блюпринта.
- `add_variable(...)`, `create_node(...)` — модификация узлов и переменных блюпринта.

### 4.5 `UMGToolSet.UMGToolSet`
Инспекция и редактирование деревьев виджетов:
- `GetWidgetTree(WidgetBlueprintPath: str)` — получение иерархии виджетов Slate/UMG.
- `GetWidgetProperties(...)`, `SetWidgetProperties(...)` — манипуляция слотами, выравниванием и свойствами виджетов.

---

## 5. Примеры использования

### 5.1 Запуск всех тестов проекта через CLI
```bash
# Запуск всех тестов GV2 в работающем Unreal Editor
python3 Tools/MCP/run_ue_tests.py

# Запуск с подробным выводом (длительность каждого теста и логи)
python3 Tools/MCP/run_ue_tests.py -v

# Запуск тестов конкретной подсистемы
python3 Tools/MCP/run_ue_tests.py --filter "StartsWith:GV2.Runtime.Presentation"
```

### 5.2 Пакетная компиляция и сохранение виджетов
```bash
# Компиляция всех WBP в /Game/ и сохранение на диск
python3 Tools/MCP/compile_and_save_assets.py

# Компиляция конкретного виджета
python3 Tools/MCP/compile_and_save_assets.py --path "/Game/UI/Widgets/WBP_DropdownSelect"
```

### 5.3 Использование библиотеки `mcp_client.py` в Python
```python
from Tools.MCP.mcp_client import UnrealMcpClient

client = UnrealMcpClient()

# 1. Запуск тестов
client.discover_tests()
run_summary = client.run_tests_by_filter("StartsWith:GV2")
results = client.get_test_results()
print(f"Passed: {results.get('passed')}, Failed: {results.get('failed')}")

# 2. Чтение и обновление свойств DataAsset
theme_path = "/Game/TextSystem/UI/Styles/DA_UITheme_Default.DA_UITheme_Default"
props = client.call_tool(
    "editor_toolset.toolsets.object.ObjectTools",
    "get_properties",
    {"instance": {"refPath": theme_path}, "properties": ["TextSizeTokens"]}
)

# 3. Сохранение ассета
client.call_tool(
    "editor_toolset.toolsets.asset.AssetTools",
    "save_assets",
    {"asset_paths": ["/Game/TextSystem/UI/Styles/DA_UITheme_Default"]}
)
```
