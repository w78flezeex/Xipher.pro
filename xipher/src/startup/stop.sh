#!/bin/bash
# Скрипт для остановки сервера Xipher

PID_FILE="/tmp/xipher_server.pid"
PORT=21971

echo "=========================================="
echo "  Xipher - Остановка сервера"
echo "=========================================="
echo ""

# Поиск процесса по PID файлу
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Остановка процесса (PID: $PID)..."
        kill "$PID"
        
        # Ожидание завершения
        for i in {1..10}; do
            if ! ps -p "$PID" > /dev/null 2>&1; then
                break
            fi
            sleep 1
        done
        
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "Принудительное завершение..."
            kill -9 "$PID"
            sleep 1
        fi
        
        rm -f "$PID_FILE"
        echo "✓ Процесс остановлен"
    else
        echo "Процесс не найден (PID: $PID)"
        rm -f "$PID_FILE"
    fi
else
    echo "PID файл не найден, поиск процесса по порту..."
fi

# Поиск процесса по порту
PORT_PID=$(lsof -ti:$PORT 2>/dev/null || true)
if [ -n "$PORT_PID" ]; then
    echo "Найден процесс на порту $PORT (PID: $PORT_PID)"
    echo "Остановка..."
    kill "$PORT_PID" 2>/dev/null || true
    sleep 1
    
    if ps -p "$PORT_PID" > /dev/null 2>&1; then
        kill -9 "$PORT_PID" 2>/dev/null || true
    fi
    
    echo "✓ Процесс остановлен"
fi

# Поиск по имени процесса
PROCESSES=$(pgrep -f "xipher_server" 2>/dev/null || true)
if [ -n "$PROCESSES" ]; then
    echo "Найдены процессы xipher_server:"
    echo "$PROCESSES"
    echo "Остановка..."
    pkill -f "xipher_server" 2>/dev/null || true
    sleep 1
    pkill -9 -f "xipher_server" 2>/dev/null || true
    echo "✓ Процессы остановлены"
fi

# Финальная проверка
if ss -tlnp | grep -q ":$PORT "; then
    echo "⚠ Порт $PORT все еще занят"
else
    echo "✓ Порт $PORT свободен"
fi

echo ""
echo "=========================================="
echo "  Сервер остановлен"
echo "=========================================="

