#!/bin/bash
# Скрипт для проверки статуса сервера Xipher

PID_FILE="/tmp/xipher_server.pid"
PORT=21971
LOG_FILE="/var/log/xipher/server.log"

echo "=========================================="
echo "  Xipher - Статус сервера"
echo "=========================================="
echo ""

# Проверка по PID файлу
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "✓ Сервер запущен"
        echo "  PID: $PID"
        
        # Информация о процессе
        if command -v ps &> /dev/null; then
            CPU=$(ps -p "$PID" -o %cpu --no-headers 2>/dev/null | tr -d ' ')
            MEM=$(ps -p "$PID" -o %mem --no-headers 2>/dev/null | tr -d ' ')
            TIME=$(ps -p "$PID" -o etime --no-headers 2>/dev/null | tr -d ' ')
            echo "  CPU: ${CPU:-N/A}%"
            echo "  Память: ${MEM:-N/A}%"
            echo "  Время работы: ${TIME:-N/A}"
        fi
    else
        echo "✗ Сервер не запущен (PID файл существует, но процесс не найден)"
        rm -f "$PID_FILE"
    fi
else
    echo "✗ Сервер не запущен (PID файл не найден)"
fi

echo ""

# Проверка порта
if ss -tlnp 2>/dev/null | grep -q ":$PORT " || lsof -Pi :$PORT -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo "✓ Порт $PORT слушается"
    PORT_PID=$(lsof -ti:$PORT 2>/dev/null || ss -tlnp 2>/dev/null | grep ":$PORT " | grep -oP 'pid=\K[0-9]+' | head -1)
    if [ -n "$PORT_PID" ]; then
        echo "  PID процесса на порту: $PORT_PID"
    fi
else
    echo "✗ Порт $PORT не слушается"
fi

echo ""

# Последние логи
if [ -f "$LOG_FILE" ]; then
    echo "Последние строки лога:"
    echo "---"
    tail -5 "$LOG_FILE" 2>/dev/null || echo "Лог файл пуст или недоступен"
    echo "---"
else
    echo "Лог файл не найден: $LOG_FILE"
fi

echo ""
echo "=========================================="

