#!/usr/bin/env bash
# ==============================================================================
# Скрипт архивации проекта GV2 для переноса на другую рабочую станцию.
# Исключает тяжелые кэши сборки, временные файлы и IDE-артефакты,
# сохраняя при этом .git, историю коммитов и исходные файлы проекта.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_NAME="$(basename "${PROJECT_DIR}")"
PARENT_DIR="$(dirname "${PROJECT_DIR}")"

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
DEFAULT_ARCHIVE="${PARENT_DIR}/${PROJECT_NAME}_clean_${TIMESTAMP}.tar.gz"
OUTPUT_ARCHIVE="${DEFAULT_ARCHIVE}"
DRY_RUN=0

usage() {
    cat << HELP
Использование: $0 [ОПЦИИ]

Опции:
  -o, --output ПУТЬ   Путь к результирующему архиву (.tar.gz)
                      (по умолчанию: ../${PROJECT_NAME}_clean_<TIMESTAMP>.tar.gz)
  -n, --dry-run       Показать список исключений и оценку размера без создания архива
  -h, --help          Показать эту справку
HELP
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o|--output)
            OUTPUT_ARCHIVE="$2"
            shift 2
            ;;
        -n|--dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Неизвестный параметр: $1" >&2
            usage
            ;;
    esac
done

echo "======================================================================"
echo "Архивация проекта: ${PROJECT_NAME}"
echo "Исходный каталог:  ${PROJECT_DIR}"
echo "Целевой архив:     ${OUTPUT_ARCHIVE}"
echo "======================================================================"

# Проверка статуса Git
if cd "${PROJECT_DIR}" && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
    COMMITS_AHEAD="$(git rev-list --count @{u}..HEAD 2>/dev/null || echo "нет upstream")"
    UNCOMMITTED_CHANGES="$(git status --porcelain | wc -l)"
    echo "Ветка Git:         ${CURRENT_BRANCH}"
    echo "Коммитов впереди:  ${COMMITS_AHEAD}"
    if [[ "${UNCOMMITTED_CHANGES}" -gt 0 ]]; then
        echo "Внимание:          В рабочей директории есть ${UNCOMMITTED_CHANGES} незакоммиченных изменений (они будут включены в архив)."
    fi
else
    echo "Внимание: Каталог .git не найден!" >&2
fi
echo "----------------------------------------------------------------------"

# Список исключений (соответствует .gitignore и кэшам сборки).
# Шаблоны НЕ привязаны к корню проекта: tar по умолчанию сопоставляет их с любым
# хвостом пути (--no-anchored), поэтому "Binaries" исключает и GV2/Binaries, и
# вложенные копии вроде GV2/.claude/worktrees/*/Binaries. Прежняя форма
# "${PROJECT_NAME}/Binaries" срабатывала только на верхнем уровне и пропускала в
# архив 729 МБ агентских worktree и 32 МБ Headless/build -- примерно половину
# итогового объёма.
EXCLUDES=(
    --exclude="Binaries"
    --exclude="DerivedDataCache"
    --exclude="Intermediate"
    --exclude="Saved"
    --exclude="build"
    --exclude="cmake-build-*"
    --exclude=".claude/worktrees"
    --exclude=".idea"
    --exclude=".vs"
    --exclude=".codex"
    --exclude=".agents"
    --exclude="*.gen.h"
    --exclude="__pycache__"
    --exclude="*.pyc"
    --exclude="*.so"
    --exclude="*.debug"
    --exclude="*.sym"
    --exclude="*.log"
    --exclude="*.tmp"
    --exclude="*.tar.gz"
    --exclude="*.tar.zst"
    --exclude="*.zip"
)

if [[ "${DRY_RUN}" -eq 1 ]]; then
    echo "[DRY-RUN] Исключаемые шаблоны:"
    for exc in "${EXCLUDES[@]}"; do
        echo "  ${exc}"
    done
    echo ""
    echo "Оценка чистого объёма каталога..."
    cd "${PARENT_DIR}"
    du -sh "${EXCLUDES[@]}" "${PROJECT_NAME}"
    echo "[DRY-RUN] Архив не создавался."
    exit 0
fi

START_TIME="$(date +%s)"

# Создание архива из родительского каталога (чтобы архив содержал корневую папку GV2/)
cd "${PARENT_DIR}"

# Использовать pigz (многопоточный gzip), если он установлен, иначе обычный gzip
COMPRESS_OPT="-z"
if command -v pigz >/dev/null 2>&1; then
    COMPRESS_OPT="--use-compress-program=pigz"
    echo "Используется многопоточный компрессор: pigz"
else
    echo "Используется стандартный компрессор: gzip"
fi

echo "Создание архива..."
tar "${COMPRESS_OPT}" -cf "${OUTPUT_ARCHIVE}" "${EXCLUDES[@]}" "${PROJECT_NAME}"

END_TIME="$(date +%s)"
DURATION=$((END_TIME - START_TIME))
ARCHIVE_SIZE="$(du -h "${OUTPUT_ARCHIVE}" | cut -f1)"

echo "----------------------------------------------------------------------"
echo "Архивация успешно завершена за ${DURATION} сек."
echo "Файл архива: ${OUTPUT_ARCHIVE} (${ARCHIVE_SIZE})"
echo ""
echo "Команда для распаковки на новой рабочей станции:"
echo "  tar -xzf $(basename "${OUTPUT_ARCHIVE}") -C /целевой/каталог/"
echo "======================================================================"
