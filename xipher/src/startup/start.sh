#!/bin/bash
# Скрипт для запуска сервера Xipher

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
for _ in 1 2 3 4; do
    PROJECT_DIR="$(dirname "$PROJECT_DIR")"
    if [ -f "$PROJECT_DIR/CMakeLists.txt" ]; then
        break
    fi
done
if [ ! -f "$PROJECT_DIR/CMakeLists.txt" ]; then
    echo "ОШИБКА: Не удалось найти корень проекта (CMakeLists.txt) от SCRIPT_DIR=$SCRIPT_DIR" >&2
    exit 1
fi
BUILD_DIR="$PROJECT_DIR/build"
SERVER_BIN="$BUILD_DIR/xipher_server"
PID_FILE="/tmp/xipher_server.pid"
LOG_FILE="/var/log/xipher/server.log"
PORT=21971

# Optional DB schema/migration + channel owner backfill before start
# Enable via: XIPHER_RUN_DB_FIX=1 DB_NAME=xipher ./startup/start.sh
XIPHER_RUN_DB_FIX="${XIPHER_RUN_DB_FIX:-0}"
# Keep default DB aligned with server defaults unless overridden
DB_NAME="${DB_NAME:-xipher}"

# Проверка существования бинарника
if [ ! -f "$SERVER_BIN" ]; then
    echo "ОШИБКА: Сервер не найден: $SERVER_BIN"
    echo "Запустите сначала: ./startup/build.sh"
    exit 1
fi

# Проверка, не запущен ли уже сервер
if [ -f "$PID_FILE" ]; then
    OLD_PID=$(cat "$PID_FILE")
    if ps -p "$OLD_PID" > /dev/null 2>&1; then
        echo "Сервер уже запущен (PID: $OLD_PID)"
        echo "Для остановки используйте: ./startup/stop.sh"
        exit 1
    else
        rm -f "$PID_FILE"
    fi
fi

# Проверка порта
if lsof -Pi :$PORT -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo "ОШИБКА: Порт $PORT уже занят"
    echo "Остановите другой процесс или измените порт"
    exit 1
fi

# Создание директории для логов
mkdir -p "$(dirname "$LOG_FILE")"
touch "$LOG_FILE"
chmod 644 "$LOG_FILE"

echo "=========================================="
echo "  Xipher - Запуск сервера"
echo "=========================================="
echo ""
echo "Порт: $PORT"
echo "Лог: $LOG_FILE"
echo ""

# Optional DB fix (non-fatal by default; can be made fatal by XIPHER_DB_FIX_STRICT=1)
if [ "$XIPHER_RUN_DB_FIX" = "1" ]; then
    echo "[DB] Running channel owner/schema fix (DB_NAME=${DB_NAME})..."
    XIPHER_DB_FIX_STRICT="${XIPHER_DB_FIX_STRICT:-0}"
    if DB_NAME="${DB_NAME}" "$SCRIPT_DIR/fix_channel_owner.sh"; then
        echo "✓ [DB] Fix applied"
    else
        echo "⚠ [DB] Fix failed"
        if [ "$XIPHER_DB_FIX_STRICT" = "1" ]; then
            echo "ОШИБКА: DB fix failed and XIPHER_DB_FIX_STRICT=1"
            exit 1
        fi
    fi
    echo ""
fi

# Запуск сервера
cd "$PROJECT_DIR"
# Load optional environment overrides for server secrets/config.
ENV_FILE="${XIPHER_ENV_FILE:-$PROJECT_DIR/.env.server}"
if [ -f "$ENV_FILE" ]; then
    set -a
    # shellcheck disable=SC1090
    . "$ENV_FILE"
    set +a
fi
# Pass DB name through to server (it reads XIPHER_DB_NAME env)
export XIPHER_DB_NAME="${XIPHER_DB_NAME:-$DB_NAME}"
nohup "$SERVER_BIN" "$PORT" >> "$LOG_FILE" 2>&1 &
SERVER_PID=$!

# Сохранение PID
echo $SERVER_PID > "$PID_FILE"

# Ожидание запуска
sleep 2

# Проверка, что процесс запущен
if ps -p "$SERVER_PID" > /dev/null 2>&1; then
    echo "✓ Сервер успешно запущен"
    echo "  PID: $SERVER_PID"
    echo "  Порт: $PORT"
    echo "  Лог: $LOG_FILE"
    echo ""
    echo "Проверка статуса:"
    if ss -tlnp | grep -q ":$PORT "; then
        echo "  ✓ Порт $PORT слушается"
    else
        echo "  ⚠ Порт $PORT не слушается (возможно, еще запускается)"
    fi
    echo ""
    echo "Для остановки: ./startup/stop.sh"
    echo "Для просмотра логов: tail -f $LOG_FILE"
else
    echo "ОШИБКА: Сервер не запустился"
    echo "Проверьте логи: $LOG_FILE"
    rm -f "$PID_FILE"
    exit 1
fi

echo "=========================================="
