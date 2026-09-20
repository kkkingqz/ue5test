# uectl 0.2.2

`uectl` — stdlib-only Python CLI для Agent Canvas, `ue-build-service` и отдельного `ue-editor-control` внутри GPU/Selkies контейнера Unreal Editor.

Аутентификации намеренно нет. Порты `8765` и `8766` должны быть доступны **только** во внутренней Docker-сети `ue-control` и не публиковаться в LAN.

## Состав пакета

- `uectl` / `uectl.py` — CLI клиента.
- `ue-editor-control` / `ue_editor_control.py` — узкий daemon управления `UnrealEditor` (код daemon без изменений относительно 0.2.0).
- `tests/` — regression-тесты CLI, HTTP, project/worktree context и lifecycle Editor.
- `Dockerfile.ue-editor-control` — пример производного Selkies image с установленным daemon.
- `supervisor/ue-editor-control.conf` — supervisor program для daemon.
- `compose-delta.yml` — изменения Compose.
- `BUILD-SERVICE-API-v1.md` — нормативный frozen wire/API contract для будущего `ue-build-service`.
- `uectl-container-design-ru.md` — обновлённый общий design-doc.

## Что исправлено в 0.2.x

- Editor после рестарта control-daemon восстанавливается сканированием `/proc`.
- Ручной `UnrealEditor` тоже обнаруживается; даже если проект нельзя определить, второй Editor автоматически не запускается.
- `editor start` проверяет, что процесс не завершился сразу после запуска.
- `editor stop` по умолчанию project-scoped; административный override — `--any`.
- transient HTTP errors имеют retry с exponential backoff.
- create-job повторяется с тем же `X-UECTL-Request-ID`.
- `--request-id` позволяет безопасно продолжить неоднозначно завершившийся create request.
- JSON client-error после создания job сохраняет `job_id` и `request_id`.
- `editor restart` намеренно **не retry-ится автоматически**, потому что операция не идемпотентна.
- исправлена обработка JSON error-body, который не является object.
- отсутствие `ready` в `/v1/status` теперь считается `ready=false` и даёт exit code 1.
- отсутствующий project в Editor API возвращает 404, а не 500.
- валидируется `Content-Length`; request body имеет timeout.
- stdout-log Editor по умолчанию хранится в writable home и имеет copy-truncate rotation.

Дополнительно в `uectl 0.2.1`:

- control-plane HTTP полностью игнорирует `HTTP_PROXY`/`HTTPS_PROXY`;
- HTTP redirect не выполняются, включая redirect на другой host;
- `IncompleteRead` и другие `http.client.HTTPException` считаются retryable transport failure;
- исчерпанные `5xx`/`408`/`425`/`429` возвращают exit code `3`;
- `UECTL_EDITOR_HTTP_TIMEOUT` отделён от обычного build timeout и по умолчанию равен 30 s;
- live-log использует короткий `UECTL_LOG_HTTP_TIMEOUT` и не делает собственный retry/backoff, поэтому отказ `/logs` не задерживает polling job metadata;
- Ctrl+C в create/wait сохраняет `request_id`, а после получения job — ещё и `job_id`; remote job автоматически не отменяется;
- валидируются endpoint URL, числовые параметры и `job-id`;
- неизвестный job state считается protocol error вместо бесконечного ожидания;
- malformed optional metadata (`engines: null`, некорректный `duration_ms`) не вызывает traceback;
- exit code Editor зависит от ожидаемого состояния (`start/restart -> running`, `stop -> stopped`);
- добавлена команда `uectl version`.

Дополнительно в `uectl 0.2.2`:

- frozen Build Service API version `1`;
- все запросы несут `User-Agent`, `X-UECTL-API-Version` и `X-UECTL-Client-Version`;
- create response принимает только нормативное поле `job_id`;
- retry HTTP ограничен `408/425/429/500/502/503/504`;
- `501`, `507` и прочие non-transient errors автоматически не retry-ятся;
- job IDs `.` и `..` запрещены;
- metadata job проверяется на совпадение `job_id/project/workspace/operation`;
- live logs используют byte-offset endpoint `?offset=` + `X-UECTL-Next-Offset` + snapshot `X-UECTL-Log-Size`;
- terminal jobs и `uectl logs` дочитывают offset chunks до EOF;
- `uectl logs -f` теперь, как и build wait, считает job metadata authoritative, а log delivery — best-effort;
- timeout при чтении HTTP error body не приводит к traceback.

## Установка `uectl` в Agent Canvas

```bash
install -m 0755 uectl /usr/local/bin/uectl
```

Runtime: Python 3.9+; сторонних пакетов нет.

Переменные окружения:

```text
UECTL_ENDPOINT=http://ue-build-service:8765
UECTL_EDITOR_ENDPOINT=http://ue-editor:8766
UECTL_PROJECT_ROOT=/projects
UECTL_WORKTREE_ROOT=/worktrees
UECTL_HTTP_TIMEOUT=10
UECTL_EDITOR_HTTP_TIMEOUT=30
UECTL_LOG_HTTP_TIMEOUT=1
UECTL_POLL_INTERVAL=0.5
UECTL_RETRY_ATTEMPTS=5
UECTL_RETRY_BACKOFF=0.25
```

## Команды build service

```bash
uectl status
uectl version

uectl build
uectl build --target editor
uectl build --target game --configuration Shipping
uectl plugin-build MyTools
uectl test --filter MyGame
uectl cook --platform Linux
uectl package --platform Linux --configuration Development
uectl clean

uectl logs <job-id>
uectl logs -f <job-id>
uectl cancel <job-id>
```

Контекст определяется из cwd:

```text
/projects/MyGame/...                  -> project=MyGame, workspace=main
/worktrees/MyGame/codex-a17f/...      -> project=MyGame, workspace=codex-a17f
```

Явный контекст:

```bash
uectl build --project MyGame --workspace codex-a17f
```

### Idempotency / retry

Каждый create-job получает:

```text
X-UECTL-Request-ID: <id>
```

При timeout/disconnect или HTTP `408`, `425`, `429`, `500`, `502`, `503`, `504` клиент повторяет запрос с **тем же** request-id. Прочие `5xx` автоматически не retry-ятся.

Если все попытки исчерпаны, request-id выводится в ошибке. Его можно безопасно использовать повторно:

```bash
uectl build --request-id 123e4567-e89b-12d3-a456-426614174000
```

Это требует от `ue-build-service` предусмотренного контрактом поведения: повторный create с тем же request-id должен возвращать уже существующий job, а не создавать новый.

После того как `job_id` уже получен, окончательная ошибка связи в `--json` режиме выглядит примерно так:

```json
{"state":"client_error","job_id":"4b44de37","request_id":"...","error":"..."}
```

Таким образом агент не теряет идентификатор продолжающейся серверной сборки.

При Ctrl+C после получения `job_id` JSON-режим возвращает, например:

```json
{"state":"client_interrupted","job_id":"4b44de37","request_id":"...","job_continues":true}
```

Если Ctrl+C произошёл во время create до получения ответа, возвращается `request_id` и `job_may_exist:true`. Это позволяет повторить create с тем же `--request-id`. Удалённый job при Ctrl+C автоматически не отменяется.

### JSON output

```bash
uectl build --json
```

Итоговый JSON идёт в stdout, streamed build log — в stderr. `--no-stream` отключает live log.

## Exit codes

```text
0   успешно
1   общая ошибка клиента/сервиса или service ready=false
2   некорректный запрос/контекст
3   сервис недоступен после retry
4   job отменён
6   UE build/AutomationTool failure без сохраняемого remote code
10..255 remote exit code сохраняется
130 Ctrl+C
```

## Build Service API

Нормативный контракт: `BUILD-SERVICE-API-v1.md`. API version: `1`. Все control-plane запросы несут:

```text
User-Agent: uectl/0.2.2
X-UECTL-API-Version: 1
X-UECTL-Client-Version: 0.2.2
```

```text
GET  /v1/status
POST /v1/jobs/build
POST /v1/jobs/plugin-build
POST /v1/jobs/test
POST /v1/jobs/cook
POST /v1/jobs/package
POST /v1/jobs/clean
GET  /v1/jobs/<id>
GET  /v1/jobs/<id>/logs?offset=<byte-offset>
POST /v1/jobs/<id>/cancel
```

Минимальный create response:

```json
{"job_id":"4b44de37"}
```

Terminal status:

```json
{"job_id":"4b44de37","state":"succeeded","exit_code":0}
```

`state`: `created`, `validated`, `queued`, `running`, `succeeded`, `failed`, `cancelled`. Неизвестное состояние считается несовместимым/повреждённым ответом сервера.

Live streaming является best-effort: `uectl build` сначала опрашивает authoritative job metadata, а `/logs` читает максимум одной короткой попыткой на poll. Ошибка live-log выдаёт warning, но не меняет результат уже завершившегося job.

`v0.2.2` использует нормативный byte-offset protocol: сервер возвращает только новый хвост, `X-UECTL-Next-Offset` и snapshot EOF `X-UECTL-Log-Size`. Для переходного pre-v1 сервера клиент сохраняет безопасный legacy fallback на полный `/logs`, но новый `ue-build-service` обязан реализовать offset contract. Полный контракт находится в `BUILD-SERVICE-API-v1.md`.

# UE Editor control

## CLI

```bash
uectl editor status
uectl editor start
uectl editor start --project MyGame

# По умолчанию stop защищён ожидаемым проектом:
uectl editor stop
uectl editor stop --project MyGame
uectl editor stop --force

# Административный override, когда cwd не относится к проекту:
uectl editor stop --any
uectl editor stop --any --force

uectl editor restart
uectl editor restart --project MyGame
uectl editor restart --force
```

`editor restart` не имеет автоматического POST-retry. Если HTTP response потерян, сначала выполнить:

```bash
uectl editor status
```

и только после проверки состояния решать, нужен ли ещё один restart.

## Поведение daemon

API:

```text
GET  /v1/editor/status
POST /v1/editor/start
POST /v1/editor/stop
POST /v1/editor/restart
```

`start` принимает только:

```json
{"project":"MyGame"}
```

Daemon сам разрешает:

```text
/projects/MyGame/*.uproject
```

и запускает фиксированный:

```text
$UE_EDITOR_ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor
```

`filesystem path`, binary path и shell command от клиента не принимаются.

### Восстановление после рестарта daemon

Если `ue-editor-control` перезапущен, он сканирует `/proc` и ищет процесс, чей `/proc/<pid>/exe` соответствует настроенному `UnrealEditor`.

Если в cmdline есть root `.uproject`, daemon восстанавливает project. Если Editor был запущен вручную без project path, состояние будет `running` с неизвестным project; новый `start` блокируется. `stop --any` остаётся доступным как явный административный override.

Если одновременно обнаружено несколько `UnrealEditor`, `status` возвращает `state=conflict`, а daemon не выбирает процесс автоматически.

### Startup/stop

После `Popen` daemon ждёт `UE_EDITOR_STARTUP_GRACE` и проверяет, что Editor не завершился сразу. Быстрый exit возвращается как ошибка start.

Для managed process используется отдельная process group/session. `stop` посылает SIGTERM и ждёт `UE_EDITOR_STOP_TIMEOUT`; `--force` после timeout применяет SIGKILL. Для восстановленного ручного процесса daemon использует его process group только если PGID совпадает с PID, иначе сигнализирует только main PID.

## Переменные `ue-editor-control`

```text
UE_EDITOR_CONTROL_HOST=0.0.0.0
UE_EDITOR_CONTROL_PORT=8766
UE_EDITOR_PROJECT_ROOT=/projects
UE_EDITOR_ENGINE_ROOT=/opt/ue/5.8
UE_EDITOR_LOG=/home/ubuntu/.local/state/ue-editor/UnrealEditor.log
UE_EDITOR_STOP_TIMEOUT=15
UE_EDITOR_STARTUP_GRACE=0.5
UE_EDITOR_REQUEST_TIMEOUT=5
UE_EDITOR_LOG_MAX_MB=64
UE_EDITOR_LOG_BACKUPS=3
UE_EDITOR_LOG_ROTATE_INTERVAL=30
```

Лог ротируется copy-truncate, поэтому у уже запущенного Editor с inherited `O_APPEND` descriptor запись продолжается в тот же pathname после truncate. Это служебный stdout/stderr daemon log; штатные UE project logs остаются отдельными.

# Интеграция с Selkies / supervisor

`expose: 8766` сам по себе **не запускает daemon**. В пакет поэтому добавлен supervisor program.

Для supervisor-based Selkies image:

```bash
docker build \
  --build-arg BASE_IMAGE='<ваш-selkies-image:tag>' \
  -f Dockerfile.ue-editor-control \
  -t local/ue-editor-control:0.2.0 .
```

`supervisor/ue-editor-control.conf` запускает daemon как `ubuntu`, то есть под тем же пользователем, для которого в исходном дизайне используется `/home/ubuntu`. Если конкретный Selkies image использует другого desktop user, изменить `user=` и `HOME=` в этом файле перед сборкой.

Daemon наследует графическое окружение supervisor/container (`DISPLAY`, `XAUTHORITY` и т.п.). Сам `UnrealEditor` стартует только после `uectl editor start`.

Порт `8766` **не публиковать** через `ports:`.

# Тесты

В этой среде общий pytest-процесс для всех HTTP regression tests иногда зависает на teardown локальных `ThreadingHTTPServer`, поэтому проверенные команды запускают группы отдельно:

```bash
PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 python -m pytest -q \
  tests/test_cli_bootstrap.py tests/test_context.py \
  tests/test_exit_codes.py tests/test_editor_control.py

PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 python -m pytest -q tests/test_cli_http.py
PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 python -m pytest -q tests/test_cli_hardening_021.py
PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 python -m pytest -q tests/test_api_v1_022.py
```

Это 63 regression tests. `pytest` нужен только для тестов; runtime `uectl` остаётся stdlib-only.
